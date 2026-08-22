# Contributing to LumaOS

LumaOS is an experimental operating system project focused on building a gaming-first platform from scratch.

## Development Philosophy — AI-First

During the experimental phase, LumaOS is developed primarily with AI assistance. AI is used throughout the development process for:

- Writing and reviewing code
- Prototyping
- Explaining low-level concepts
- Designing system architectures
- Debugging
- Generating tests
- Documentation
- Research
- Optimization
- Development automation

The project creator focuses on:

- Project vision and direction
- Product and UX/UI direction
- The gaming experience
- Functional decisions
- Testing and validation
- Roadmap and priorities

This workflow may evolve as the project grows.

## Development Priorities

LumaOS is **not trying to recreate Windows or Linux**.

Every component should answer one question:

> **Does this directly help build a better gaming-focused operating system?**

If the answer is no, the component should be simplified, postponed, or removed.

## Contributing Code

When contributing code:

1. Keep changes focused and easy to review.
2. Preserve the existing low-level architecture unless there is a strong reason to change it.
3. Avoid introducing unnecessary dependencies.
4. Keep the kernel freestanding and compatible with the project's current toolchain.
5. Test changes in QEMU/OVMF whenever possible.
6. Update the relevant documentation when architecture or behavior changes.
7. Make sure existing regression tests still pass.

## Commit Guidelines

Use short, descriptive commit messages. Examples:

```text
feat: add keyboard ring buffer
fix: release user stack pages on exit
docs: update memory architecture
refactor: simplify scheduler task selection
```

## Pull Requests

A good pull request should explain:

- What changed
- Why it changed
- How it was tested
- Any known limitations or follow-up work

For kernel changes, include relevant QEMU output when it helps demonstrate correctness.

## Reporting Bugs

When reporting a bug, include:

- Host OS
- Toolchain versions when relevant
- QEMU / OVMF version
- Exact reproduction steps
- Relevant build output
- Relevant QEMU output or crash information

## Project Direction

LumaOS is intentionally built around a long-term gaming-first vision: minimal overhead, direct hardware control, fast startup, native gaming APIs, and a console-like user experience.

Contributions that support this direction are especially welcome.
