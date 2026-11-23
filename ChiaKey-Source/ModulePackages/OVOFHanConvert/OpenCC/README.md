# OpenCC dictionaries

Unmodified copies of the dictionaries behind OpenCC's `tw2s` configuration,
taken from [BYVoid/OpenCC](https://github.com/BYVoid/OpenCC) at tag
`ver.1.4.2` (commit `025f371dc76b598d77384fbdab90c937471844d8`), under the
Apache License 2.0 in `LICENSE`.

| File | Upstream path | Role |
| --- | --- | --- |
| `CJK_Compatibility_Ideographs.txt` | `data/dictionary/` | normalization |
| `TWVariants.txt` | `data/dictionary/` | reversed into `TWVariantsRev` |
| `TWVariantsRevPhrases.txt` | `data/dictionary/` | Taiwan variants, phrase exceptions |
| `TSPhrases.txt` | `data/dictionary/` | segmentation and phrase conversion |
| `TSCharacters.txt` | `data/dictionary/` | character conversion |
| `STCharacters.txt` | `data/dictionary/` | generator check only, not shipped |
| `tw2s-testcases.tsv` | `test/testcases/testcases.json` | the `tw2s` cases, as input/expected |

`TSCharactersExt` is left out: `tw2s` marks it `may_output_tofu`, and OpenCC
skips such dictionaries by default.

ChiaKey's own changes live in `../TaiwanToSimplifiedOverrides.txt`.
`Scripts/generate-taiwan-to-simplified-table.py` combines both into
`../TaiwanToSimplifiedTable.inc`; rerun it after updating either.
