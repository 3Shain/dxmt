
# Contributing to DXMT

## Commit Messages

We use semantic commit messages.

Format: `<type>(<scope>): <subject>`

Valid types:
- feat: A newly added feature
- fix: Just a fix
- refactor: Any changes to the implementation that do not introduce new functionality
- chore: Any changes to the code that do not change the implementation (e.g. formatting, adding comments)
- docs: For documentation only (typically .md files)
- build: For build script only
- ci: For CI script only

The scope can be any sub-directory in `src/`. If a patch involves multiple scopes, separate them by comma `, `. Scopes can be omitted if there are more than 4, but please try your best to constrain your changes into one single scope. For docs/build/ci, no need to add scope.

The subject is a setence describing the change. Do not capitalize the first letter.

And most importantly, please **sign off** your commit with your legal name.

## AI Policy

We cannot accept contributions made or co-authored by AI/LLM (Large Language Model).

You are still free to use AI to do your own research and share your findings with others (including the developers, but please don't create a PR).