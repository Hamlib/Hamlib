# Welcome to Hamlib docs contributing guide <!-- omit in toc -->

Thank you for investing your time in contributing to the Hamlib project!

Read our [Code of Conduct](./CODE_OF_CONDUCT.md) to keep our community
approachable and respectable.

In this guide you will get an overview of the contribution workflow from
opening an issue, creating a PR, reviewing, and merging the PR.

## New contributor guide

To get an overview of the project, read the [README](README.md) file. Here are
some GitHub resources to help you get started with Free Software/open source
contributions:

- [Finding ways to contribute to open source on GitHub](https://docs.github.com/en/get-started/exploring-projects-on-github/finding-ways-to-contribute-to-open-source-on-github)
- [Set up Git](https://docs.github.com/en/get-started/getting-started-with-git/set-up-git)
- [GitHub flow](https://docs.github.com/en/get-started/using-github/github-flow)
- [Collaborating with pull requests](https://docs.github.com/en/github/collaborating-with-pull-requests)


## Getting started

The [README.developer](README.developer) is a detailed overview of contributing
to the Hamlib project.

For those primarily interested in testing,
[README.betatester](README.betatester) will serve as a guide.

### Issues

#### Create a new issue

If you wish to report a problem with a specific radio or have an idea for an
enhancement of Hamlib, take a look at the [Hamlib issue
tracker](https://github.com/Hamlib/Hamlib/issues).

Use these GitHub documents to learn [how to search if an issue already
exists](https://docs.github.com/en/github/searching-for-information-on-github/searching-on-github/searching-issues-and-pull-requests#search-by-the-title-body-or-comments).
If a related issue doesn't exist, you can open a new issue using a relevant
[issue form](https://github.com/github/docs/issues/new/choose).


#### Solve an issue

Scan through our [existing issues](https://github.com/Hamlib/Hamlib/issues) to
find one that interests you. You can narrow down the search using `labels` as
filters. See "[Label
reference](https://docs.github.com/en/contributing/collaborating-on-github-docs/label-reference)"
for more information.  As a general rule, we don’t assign issues to anyone
unless interest is expressed in doing so as a signal it is being worked on.
If you find an issue to work on, you are welcome to open a PR with a fix and
reference the issue with the `#dddd` syntax in the text of your PR.  This will
link the two.

### Make Changes

#### Make changes locally

1. Fork the repository.
  - Using GitHub Desktop:
    - [Getting started with GitHub
      Desktop](https://docs.github.com/en/desktop/installing-and-configuring-github-desktop/getting-started-with-github-desktop)
      will guide you through setting up Desktop.
    - Once Desktop is set up, you can use it to [fork the Hamlib
      repository](https://docs.github.com/en/desktop/contributing-and-collaborating-using-github-desktop/cloning-and-forking-repositories-from-github-desktop).

  - Using the command line:
    - [Fork the Hamlib
      repository](https://docs.github.com/en/github/getting-started-with-github/fork-a-repo#fork-an-example-repository)
      so that you can make your changes without affecting the original project
      until you're ready to merge them.

2. Create a working branch and start with your changes!

### Commit your update

Commit the changes once you are satisfied with them and the code works as
intended 

**Note:** Some advice is out there to commit each time a file is saved.  This
creates a needless string of commits that might never make it into the final
merge.  Better is to test and iterate and once the code is where you want it
then commit with a descriptive message.  Even so, it is worthwhile to use `git
rebase` to reorder, drop, squash, fixup, or one of the other actions it
provides to make you look like a code genius!

### Pull Request

After you've tested your commits to ensure they compile without errors, or
warnings (if possible), "push" your commits to your GitHub repository, i.e.
your *fork* of Hamlib.  Once the push completes successfully the GitHub server
will return a URL that can be opened in your Web browser to create the Pull
Request (PR).

### PR review

Your PR will be reviewed by at least the Hamlib maintainer before being merged
into the master branch.  Discussion may take place in the PR about one or more
commits and it's possible that the process will result in you being asked to
do something differently or consider another approach to solving a problem.
This is the process of collaborative development.

### Your PR is merged!

Congratulations :tada::tada: The Hamlib team thanks you :sparkles:.

Once your PR is merged, your contributions will be publicly visible on
[Hamlib](https://github.com/Hamlib/Hamlib).
