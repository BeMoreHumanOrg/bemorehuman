<!---
SPDX-FileCopyrightText: 2022 Brian Calhoun <brian@bemorehuman.org>
SPDX-License-Identifier: MIT
-->
# How to contribute
Thank you for wanting to help bemorehuman get better!

## How to file a bug report or suggest a new feature 
Head over to the bemorehuman [issues](https://github.com/BeMoreHumanOrg/bemorehuman/issues) section and add a new issue. Github will ask you if it's a new feature or bug report and show you the appropriate template.

## If you don't know whether to make a new issue or not
Just ask! You can ask us questions on our [discussions](https://github.com/BeMoreHumanOrg/bemorehuman/discussions) page.

## Types of contributions we're looking for
- Suggest documentation! As in, what would you like to see? A wiki? On Github? Gists? Text files in the repo? Let us know in the [discussions](https://github.com/BeMoreHumanOrg/bemorehuman/discussions). 
- Feedback on [issues](https://github.com/BeMoreHumanOrg/bemorehuman/issues). We welcome constructive feedback on any of the issues.
- Commenting or asking questions or answering questions in our [discussions](https://github.com/BeMoreHumanOrg/bemorehuman/discussions) is quite helpful!
- Feel free to tackle an issue if you like. If you let us know your intention first before you start coding, that would be helpful and save everyone some time. 

### Fork & create a branch
If you find an issue you think you can fix, then please fork bemorehuman and create a branch with a descriptive name.
A good branch name would be (where issue #66 is the ticket you're working on):

    git checkout -b 66-add-shiny-buttons
    
Then do the fix, test it (see below), and submit a pull request in the normal way.
    
## Technical requirements to make a code contribution
- Browse the code to get a feel for the amount and style of commenting. It's important to have a consistent feel so that developers don't have a jarring or confusing experience.
- Be on Linux. Or not! We've currently only tested on Linux and the threading stuff probably wouldn't work on OS's without phtreads. Maybe other stuff wouldn't work? Try it out!
- Make sure that after your contribution, running any of the affected binaries produces clean, error-free output. This may seem obvious, but it's good to spell things out.
- Ensure your submission does not significantly diverge from the standard accuracy values mentioned in the README: Mean Absoluete Error (MAE) is 1.895062 for "bemorehuman exp10"
  - Please see the README for more details.
- Come up with your own simple testing steps you can describe in your pull request when you're ready to submit the PR.  
    
## Roadmap/vision for the project
We want bemorehuman to be the default way that people get recommendations online. 

The plan is to have bemorehuman being used by developers in every country on the planet. We'd like recommendations tailored to the receiver to be used as widely as possible in many different situations and use cases. 

There is also some info in the [MANIFESTO](/MANIFESTO) file that relates to how we see the world.

## How to get in touch
If you want to contact me personally, feel free to contact me @unbrand via email/website mentioned on my Github profile. If you want to address the wider group, try the [discussions](https://github.com/BeMoreHumanOrg/bemorehuman/discussions).   
