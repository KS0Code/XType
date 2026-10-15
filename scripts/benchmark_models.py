#!/usr/bin/env python3
"""Compare Ollama models as XType autocomplete backends.

Sends each model the same request the fcitx5 engine sends — /api/chat with
the system prompt and the text as an unfinished assistant turn, trailing
blanks dropped — then cleans the output the way
the engine does (prompt-echo cut, leading-blank trim, echo strip, trailing
trim) and scores it against the known continuation. --legacy reproduces the
engine before those two fixes (prompt sent as typed, no prompt-echo cut).

Cases: the built-in set below, plus --cases FILE.jsonl
({"category", "context", "expected"} per line; see build_bench_cases.py).

Usage:
    python3 scripts/benchmark_models.py qwen2.5:3b qwen3:4b-instruct-2507-q4_K_M
    python3 scripts/benchmark_models.py --cases /tmp/cases.jsonl --out /tmp/bench.json MODEL ...

Stdlib only.
"""

import argparse
import json
import re
import statistics
import time
import urllib.request

HOST = "http://localhost:11434"

# Mirrors kBaseSystemPrompt + "About the user:" in the engine (prompt_builder.cpp).
BASE_PROMPT = (
    "You are an inline text autocomplete engine. Continue the text you are given with a "
    "few natural words. Output ONLY the continuation. No explanations, no responses, "
    "no punctuation at the start."
)
DEFAULT_DESCRIPTION = "Senior software engineer who writes in Brazilian portuguese and English"

OPTIONS = {"num_predict": 30, "temperature": 0.3, "top_p": 0.9, "stop": [".", "!", "?", "\n"]}

# (category, context, expected continuation). Expected text is what a human
# would most likely type next; scoring compares leading words.
CASES = [
    # English — chat / email
    ("en-chat", "Thanks for the quick reply. I'll take a look at it and get back to you as soon as ", "possible"),
    ("en-chat", "Sorry for the late reply, I was out of the ", "office"),
    ("en-chat", "The meeting has been moved from Tuesday to Wednesday at 3 pm, so please update your ", "calendar"),
    ("en-chat", "Hi Maria,\nI hope you are doing well. I'm writing to follow ", "up on"),
    ("en-chat", "No worries, take your time. Let me know if you have any ", "questions"),
    # English — engineering
    ("en-dev", "Can you send me the link to the pull ", "request"),
    ("en-dev", "Before merging, please make sure all the unit tests are ", "passing"),
    ("en-dev", "The request fails intermittently, so we should add a retry with exponential ", "backoff"),
    ("en-dev", "I fixed the bug in the login flow: the session token was not being refreshed when it ", "expired"),
    ("en-dev", "I feel the suggestions this app gives are not that useful, it seems like the suggestions are not related to the text being ", "typed"),
    # English — mid-word
    ("en-midword", "I'll deploy the new version to the staging environ", "ment"),
    ("en-midword", "Please review the documentation before the next relea", "se"),
    # English — long context, needs something from earlier in the text
    ("en-long",
     "Hi team,\nQuick update on the payments migration. We are moving the settlement service from Azure "
     "Functions to Azure Container Apps, because the cold starts on Functions were adding up to two seconds "
     "to every payout request. The new deployment has been running in staging since Monday and the p95 "
     "latency dropped from 2.4 seconds to 180 milliseconds. Next week we will start routing 10 percent of "
     "production traffic to it. The main reason we chose Container ", "Apps"),
    ("en-long",
     "Hello Daniel,\nMy name is Ewerton and I'm a senior software engineer at Lumon Pay. I'm reaching out "
     "because we are evaluating vendors for card tokenization and your platform came up several times in our "
     "research. Would you have 30 minutes next week for a quick call to walk us through the API and pricing? "
     "Any day after 2 pm works for me.\nBest regards,\n", "Ewerton"),
    # Portuguese (BR)
    ("pt-chat", "Oi, tudo bem? Você consegue me mandar o relatório até o final do ", "dia"),
    ("pt-chat", "Obrigado pela ajuda, fico no ", "aguardo"),
    ("pt-chat", "Desculpa a demora para responder, estava em uma ", "reunião"),
    ("pt-chat", "Qualquer dúvida, estou à ", "disposição"),
    ("pt-chat", "Vamos marcar uma call amanhã de manhã para alinhar os próximos ", "passos"),
    ("pt-dev", "Esse problema ainda está acontecendo em produção, preciso que alguém olhe os ", "logs"),
    ("pt-midword", "A gente precisa resolver esse problema o mais rápido possí", "vel"),
    ("pt-long",
     "Olá Carlos, tudo bem?\nAqui é o Ewerton, da Lumon Pay. Estou entrando em contato porque estamos "
     "migrando o serviço de pagamentos para o Azure e gostaríamos de entender melhor como funciona o suporte "
     "de vocês para clientes enterprise. Você teria disponibilidade para uma conversa rápida na quinta-feira "
     "à tarde?\nUm abraço,\n", "Ewerton"),
]


# ── Engine-equivalent request + clean-up ─────────────────────────────────────

def payload(model, context, system_prompt):
    return {"model": model, "stream": True, "options": OPTIONS,
            "messages": [{"role": "system", "content": system_prompt},
                         {"role": "assistant", "content": context}]}


def ends_blank(ctx):
    return bool(ctx) and ctx[-1] in " \t\n"


def strip_echo_prefix(s, sent):
    """caret_context::withoutPromptEcho: drop a verbatim re-emission of the
    prompt (some chat templates return the assistant prefill in front)."""
    if not sent:
        return s
    if s.startswith(sent):
        return s[len(sent):]
    if sent.startswith(s):          # still streaming the echo
        return ""
    return s


def finalize(s, ctx, sent=None):
    """xtype.cpp done-handler: trailing trim, tail/head echo strip, leading trim.
    With `sent`, a full echo of the prompt is stripped first."""
    if sent is not None:
        s = strip_echo_prefix(s, sent)
    s = s.rstrip()
    for n in range(min(len(s), len(ctx), 80), 3, -1):
        if ctx.endswith(s[:n]):
            s = s[n:]
            break
    if len(s) >= 12 and len(ctx) >= 12 and s[:12].lower() == ctx[:12].lower():
        s = ""
    if ends_blank(ctx):
        s = s.lstrip(" \t")
    return s


def run_one(model, context, system_prompt, fixes=True):
    # caret_context::promptFor: don't end the prompt on a bare space —
    # tokenizers attach the space to the next word, so a trailing one skews
    # the model toward digits and word fragments. The real context still
    # drives the clean-up.
    sent = context.rstrip(" \t") if fixes else context
    body = payload(model, sent, system_prompt)
    req = urllib.request.Request(HOST + "/api/chat", data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    t0 = time.perf_counter()
    ttft = None
    text = ""
    final = {}
    with urllib.request.urlopen(req, timeout=120) as resp:
        for line in resp:
            line = line.strip()
            if not line:
                continue
            d = json.loads(line)
            tok = (d.get("message") or {}).get("content")
            if tok:
                # Ghost text appears once the (trimmed) suggestion is non-empty.
                shown = text + tok
                visible = strip_echo_prefix(shown, sent) if fixes else shown
                if ttft is None and (visible.lstrip(" \t") if ends_blank(context) else visible):
                    ttft = (time.perf_counter() - t0) * 1000
                text = shown
            if d.get("done"):
                final = d
                break
    total = (time.perf_counter() - t0) * 1000
    return {
        "raw": text,
        "suggestion": finalize(text, context, sent if fixes else None),
        "ttft_ms": ttft if ttft is not None else total,
        "total_ms": total,
        "prompt_tokens": final.get("prompt_eval_count", 0),
        "prompt_ms": final.get("prompt_eval_duration", 0) / 1e6,
        "gen_tokens": final.get("eval_count", 0),
        "gen_ms": final.get("eval_duration", 0) / 1e6,
    }


# ── Scoring ──────────────────────────────────────────────────────────────────

def words(s):
    return [w for w in re.split(r"\s+", re.sub(r"[^\w\s]", " ", s.lower())) if w]


def score(suggestion, expected):
    # Mid-word cases expect the rest of the word ("environ" → "ment"), so the
    # same leading-word comparison covers both.
    sw, ew = words(suggestion), words(expected)
    first_ok = bool(sw) and sw[0] == ew[0]
    matched = 0
    for a, b in zip(sw, ew):
        if a != b:
            break
        matched += 1
    return {"first_word": first_ok, "words_matched": matched, "empty": not suggestion.strip()}


def pct(xs, p):
    xs = sorted(xs)
    return xs[max(0, min(len(xs) - 1, round(p / 100 * (len(xs) - 1))))]


def loaded_info(model):
    try:
        with urllib.request.urlopen(HOST + "/api/ps", timeout=5) as r:
            for m in json.load(r).get("models", []):
                if m.get("name") == model or m.get("model") == model:
                    size, vram = m.get("size", 0), m.get("size_vram", 0)
                    return {"size_gb": size / 1e9, "gpu_pct": round(100 * vram / size) if size else 0}
    except Exception:
        pass
    return {}


def unload(model):
    body = json.dumps({"model": model, "keep_alive": 0}).encode()
    req = urllib.request.Request(HOST + "/api/generate", data=body,
                                 headers={"Content-Type": "application/json"})
    try:
        urllib.request.urlopen(req, timeout=60).read()
    except Exception:
        pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("models", nargs="+", help="Ollama model names")
    ap.add_argument("--runs", type=int, default=1, help="runs per case (default 1)")
    ap.add_argument("--cases", help="extra cases, JSONL")
    ap.add_argument("--no-builtin", action="store_true", help="skip the built-in cases")
    ap.add_argument("--description", default=DEFAULT_DESCRIPTION)
    ap.add_argument("--out", default="/tmp/xtype-bench.json")
    ap.add_argument("--legacy", action="store_true",
                    help="engine before the fixes: prompt as typed, no prompt-echo cut")
    args = ap.parse_args()

    cases = [] if args.no_builtin else [{"category": c, "context": x, "expected": e}
                                         for c, x, e in CASES]
    if args.cases:
        with open(args.cases, encoding="utf-8") as f:
            cases += [json.loads(line) for line in f if line.strip()]
    print(f"{len(cases)} cases, {args.runs} run(s) each", flush=True)

    system_prompt = BASE_PROMPT + ("\n\nAbout the user: " + args.description if args.description else "")
    results = {}

    for model in args.models:
        print(f"\n=== {model} ===", flush=True)

        t0 = time.perf_counter()
        run_one(model, "Hello, this is a warm-up request to load the ", system_prompt)
        load_s = time.perf_counter() - t0
        info = loaded_info(model)
        print(f"  load+first request: {load_s:.1f}s  {info}", flush=True)

        rows = []
        t_model = time.perf_counter()
        for i, case in enumerate(cases, 1):
            cat, ctx, exp = case["category"], case["context"], case["expected"]
            for run in range(args.runs):
                try:
                    r = run_one(model, ctx, system_prompt, not args.legacy)
                except Exception as e:  # keep going; count as an empty suggestion
                    r = {"raw": "", "suggestion": "", "ttft_ms": 0, "total_ms": 0, "error": str(e)}
                r.update(score(r["suggestion"], exp), category=cat, run=run,
                         context_tail=ctx[-45:], expected=exp)
                rows.append(r)
            if i % 100 == 0 or i == len(cases):
                acc = 100 * sum(x["first_word"] for x in rows) / len(rows)
                print(f"  {i}/{len(cases)}  first-word {acc:.0f}%  "
                      f"({time.perf_counter() - t_model:.0f}s)", flush=True)

        results[model] = {"model": model, "load_s": load_s, **info, "rows": rows}
        unload(model)
        with open(args.out, "w") as f:  # after each model, so a crash keeps the rest
            json.dump(results, f, ensure_ascii=False, indent=1)

    # ── Summary ──
    print("\n" + "=" * 112)
    print(f"{'model':46} {'1st-word':>8} {'words':>6} {'empty':>6} "
          f"{'TTFT p50':>9} {'p90':>6} {'total p50':>10} {'p90':>6} {'GPU':>5}")
    print("-" * 112)
    for res in results.values():
        rows = res["rows"]
        first = 100 * sum(r["first_word"] for r in rows) / len(rows)
        wm = statistics.mean(r["words_matched"] for r in rows)
        empty = 100 * sum(r["empty"] for r in rows) / len(rows)
        # Latency from each case's first run only: repeats hit Ollama's prompt cache.
        tt = [r["ttft_ms"] for r in rows if r["run"] == 0]
        tot = [r["total_ms"] for r in rows if r["run"] == 0]
        print(f"{res['model'][:46]:46} {first:7.0f}% {wm:6.2f} {empty:5.0f}% "
              f"{pct(tt, 50):7.0f}ms {pct(tt, 90):5.0f} {pct(tot, 50):8.0f}ms {pct(tot, 90):5.0f} "
              f"{str(res.get('gpu_pct', '?')) + '%':>5}")

    print("\nFirst-word accuracy by category:")
    cats = sorted({c["category"] for c in cases})
    print(f"{'model':46} " + " ".join(f"{c[:16]:>16}" for c in cats))
    for res in results.values():
        line = f"{res['model'][:46]:46} "
        for c in cats:
            rs = [r for r in res["rows"] if r["category"] == c]
            line += f"{100 * sum(r['first_word'] for r in rs) / len(rs):15.0f}% "
        print(line)
    print(f"\nFull results: {args.out}")


if __name__ == "__main__":
    main()
