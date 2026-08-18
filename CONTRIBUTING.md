# Contributing to Novera

Thank you for your interest in contributing to Novera! This document outlines the guidelines for contributing to the project.

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/noverapj/Server.git`
3. Create a feature branch: `git checkout -b feature/my-new-feature`
4. Make your changes
5. Submit a pull request

## Build Environment

- Visual Studio 2010 (or VS 2022 with v100 platform toolset)
- Premake5 (beta 8)
- No additional SDKs required
- Windows SDK 7.0A

## Code Style

- Use tabs for indentation (matching existing codebase)
- Keep MBCS character set (not Unicode)
- Follow existing naming conventions in each module
- Do not introduce new third-party dependencies without discussion

## Commit Guidelines

- Use clear, descriptive commit messages
- Reference issues in commit messages when applicable: `Fix crash in io3DEngine render loop #123`
- Keep commits focused — one logical change per commit

## Pull Request Process

1. Ensure your code builds successfully in both Debug and Release configurations
2. Update the README.md if your changes affect the build process or project structure
3. Ensure no `.lib` files are committed to `src/` directories — all libraries belong in `lib/` with appropriate subfolders
4. Do not commit build artifacts (`build/` directory, `.vs/`, `*.obj`, `*.pdb`, `*.ilk`)
5. Your pull request will be reviewed before merging

## Reporting Issues

- Use GitHub Issues to report bugs or request features
- Include reproduction steps, expected behavior, and actual behavior
- Specify your build configuration (Debug/Release/Shipping) and platform

## License

By contributing to Novera, you agree that your contributions will be licensed under the GNU General Public License v3.0.


