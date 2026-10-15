#!/usr/bin/env python3
"""Build autocomplete test cases for benchmark_models.py from public text.

Sources (downloaded into --data-dir):
  en-email  AESLC (Enron e-mail bodies) via the Hugging Face datasets-server
  en-short  Tatoeba English sentences
  pt-short  Tatoeba Portuguese sentences

Each case cuts a real sentence at a word boundary (or, for ~15%, inside a
word): the text before the cut is the context, the rest of the sentence up to
. ! ? or a newline is the expected continuation. E-mail cases keep the
preceding e-mail text as context (last --context-chars characters).

Usage:
    python3 -I scripts/build_bench_cases.py --data-dir /tmp/xtype-bench-data \\
        --out /tmp/xtype-bench-cases.jsonl
"""

import argparse
import bz2
import csv
import json
import os
import random
import re
import sys
import urllib.request

TATOEBA = "https://downloads.tatoeba.org/exports/per_language/{lang}/{lang}_sentences.tsv.bz2"
AESLC = ("https://datasets-server.huggingface.co/rows?dataset=Yale-LILY/aeslc"
         "&config=default&split=train&offset={offset}&length=100")

csv.field_size_limit(sys.maxsize)


def fetch(url, path):
    if not os.path.exists(path):
        print(f"  downloading {url}", file=sys.stderr)
        with urllib.request.urlopen(url, timeout=120) as r, open(path, "wb") as f:
            f.write(r.read())
    return path


def tatoeba(lang, data_dir):
    path = fetch(TATOEBA.format(lang=lang), os.path.join(data_dir, f"{lang}_sentences.tsv.bz2"))
    with bz2.open(path, "rt", encoding="utf-8") as f:
        for row in csv.reader(f, delimiter="\t", quoting=csv.QUOTE_NONE):
            if len(row) >= 3:
                yield row[2]


def aeslc(data_dir, pages):
    for p in range(pages):
        path = fetch(AESLC.format(offset=p * 100), os.path.join(data_dir, f"aeslc_{p:03}.json"))
        with open(path, encoding="utf-8") as f:
            for r in json.load(f).get("rows", []):
                yield r["row"]["email_body"]


SENT_END = re.compile(r"[.!?\n]")
BAD = re.compile(r"https?://|www\.|@|-----|\b(From|To|Cc|Subject|Sent):|[<>{}|=_*#]")


def make_case(rng, prefix_text, sentence, category, context_chars, mid_word_rate=0.15):
    """Cut `sentence` (which follows prefix_text) and return a case or None."""
    words = list(re.finditer(r"\S+", sentence))
    if len(words) < 4:
        return None
    if rng.random() < mid_word_rate:
        # Inside a word of >= 5 letters, keeping >= 2 letters on each side.
        cands = [w for w in words[1:] if re.fullmatch(r"\w{5,}", w.group())]
        if not cands:
            return None
        w = rng.choice(cands)
        cut = w.start() + rng.randint(2, len(w.group()) - 2)
        kind = "midword"
    else:
        # Before word k (k >= 2): the context ends with the space after word k-1.
        k = rng.randint(2, len(words) - 1)
        cut = words[k].start()
        kind = "word"
    context = prefix_text + sentence[:cut]
    m = SENT_END.search(sentence, cut)
    expected = sentence[cut:m.start() if m else len(sentence)].strip()
    if not re.search(r"\w", expected) or len(context.strip()) < 10:
        return None
    return {"category": f"{category}-{kind}", "context": context[-context_chars:], "expected": expected}


def short_cases(rng, sentences, category, n, context_chars):
    pool = [s for s in sentences if 25 <= len(s) <= 160 and not BAD.search(s)]
    rng.shuffle(pool)
    out = []
    for s in pool:
        c = make_case(rng, "", s, category, context_chars)
        if c:
            out.append(c)
        if len(out) >= n:
            break
    return out


def email_cases(rng, bodies, n, context_chars):
    out = []
    bodies = [b for b in bodies if 200 <= len(b) <= 4000]
    rng.shuffle(bodies)
    for body in bodies:
        body = re.sub(r"[ \t]+", " ", body)
        # Sentences (or lines) that look like prose.
        spans = [m for m in re.finditer(r"[^.!?\n]+[.!?]?", body)
                 if 30 <= len(m.group().strip()) <= 220 and not BAD.search(m.group())]
        if not spans:
            continue
        m = rng.choice(spans)
        start = m.start() + (len(m.group()) - len(m.group().lstrip()))
        c = make_case(rng, body[:start], body[start:m.end()], "en-email", context_chars)
        if c:
            out.append(c)
        if len(out) >= n:
            break
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-dir", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--emails", type=int, default=450)
    ap.add_argument("--en", type=int, default=300)
    ap.add_argument("--pt", type=int, default=450)
    ap.add_argument("--context-chars", type=int, default=1000, help="engine context_window")
    args = ap.parse_args()
    os.makedirs(args.data_dir, exist_ok=True)
    rng = random.Random(args.seed)

    cases = email_cases(rng, list(aeslc(args.data_dir, pages=12)), args.emails, args.context_chars)
    cases += short_cases(rng, tatoeba("eng", args.data_dir), "en-short", args.en, args.context_chars)
    cases += short_cases(rng, tatoeba("por", args.data_dir), "pt-short", args.pt, args.context_chars)

    with open(args.out, "w", encoding="utf-8") as f:
        for c in cases:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    by = {}
    for c in cases:
        by[c["category"]] = by.get(c["category"], 0) + 1
    print(f"{len(cases)} cases → {args.out}  {by}", file=sys.stderr)


if __name__ == "__main__":
    main()
