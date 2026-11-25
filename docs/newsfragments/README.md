# News Fragments

Towncrier fragments live in this directory as `+NAME.TYPE.md`, with `TYPE`
one of the types configured in `towncrier.toml`: `security`, `removed`,
`deprecated`, `added`, `changed`, `fixed`, `dev`, and `misc`.

Use one fragment per user-facing change. Preview the release notes with
`towncrier build --draft --version X.Y.Z`.
