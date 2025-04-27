<!---
SPDX-FileCopyrightText: 2022 Brian Calhoun <brian@bemorehuman.org>
SPDX-License-Identifier: MIT
-->
<p align="center">
<img src="/assets/logo.svg" width=15% height=15%>
</p>

## What does bemorehuman do?

bemorehuman is a recommendation engine; sometimes these things are called recommender systems.
It recommends things such as books, or movies, or beer, or music bands to people. You input
implicit or explicit ratings data or purchase data or click data, and bemorehuman will build
a model of that dataset. With the model built and loaded into RAM, you can then query the
system via a REST interface and receive recommendations in real-time. 

## Why is it useful?

We don't want to waste time getting poor quality recommedations. Often, when the word 
"recommendation" is used what's really meant is "popular thing for your demographic." If you're
lucky, that is. If you're not lucky, recommendation can easily mean "different models of
the thing you just bought" or "whatever we're paid to promote this week." Our software takes
a different view. We think it's most important to give recommendations purely based on what
the individual receiving the recs might like. This way, the traditional marketer-focused 
mechanisms can be bypassed and focus can be given to what people would find interesting or
useful.  

## How it works

Here is a simple example of how bemorehuman works:

- What do you want to recommend? Books? Movies? Classified ads? Things that work well
are things that are unique, don't change, and have a persistent id. And of course you need
the legal right to use that data for your specific purposes. For example, "red wine" is
a good category of things to recommend because there are lots of different red wines such as
"Seifried Nelson Pinot Noir 2020" which is a unique product or thing. In the installation 
instructions below we show an example of how you can recommend movies.

- The input to bemorehuman is a list of 3-value anonymous events. An event would be like:
"person 2637 gave product 312 a rating of 4." So the input file would look like:

      2637  312  4  <-- this is the event mentioned above
      311   3363 2  <-- next event
      1288  778  5  <-- next event
      ...and so on

- The ratings (it doesn't have to be ratings; we can use purchases, clicks, etc.) file from
the previous bullet is used as input to the valence generator, valgen. A valence is just a
pairwise relationship between two things. The valence indicates strength or weakness of
connectivity between those two things. We use simple statistical methods like linear
regression and correlation coefficients to determine the bond between two things.

- Once valences are generated, they get loaded into memory so that the runtime recommender,
recgen, doesn't have to compute those valences when recommending. In general, recgen will find
the most highly correlated items to what the peron already knows about. Recgen does not know
anything about the category of what it's recommending. So if recgen is recommending wines,
it won't know anything about wines, nor that it's even recommending wines. It just knows about
the valences which represent anonymous things.

- Recgen receives a request for recommendations via a REST API call. For example, your website
visitor might want to receive a recommendation for wine. You know from your user database
that this person has rated, say, 4 wines in the past. Your web server makes a POST request to
the recgen process running somewhere on your infrastructure. About, say, 25 milliseconds later
recgen responds with an ordered list of the top 20 recommended wines for your website visitor.

- Your website decides how best to display the top 2 or 3 recommendations and maybe offers a
discount on purchase of those.

- That's it! In the installation instructions below, we flesh this out a bit more with a
concrete example of recommending movies and show you how you can generate recommendations
yourself.

## What's special about bemorehuman?

We focus on making bemorehuman the best recommender it can be, based on the core principle of
focusing on the person receiving the rec and what would be interesting to them. Here's a list
of things we think make us special. Some of these things we have in common with other
recommenders, of course.

- Unique codebase - Our codebase is not modelled on anything else nor is it an
implementation of any particular recommendation technique. We wrote the code from the ground
up based on our own ideas about what's important in a recommendation.

- Focused on receiver of rec - We are not focused on the marketer/advertiser side of the
equation. We think that the only way to provide a good recommendation is to be 100% focused
on what the receiver might want. The web has too much possibility to only offer people mass-
marketed or even group-marketed content.

- Multi-platform - We've tested bemorehuman to run successfully on Linux, Mac, FreeBSD, and NetBSD.

- Recommendations based on popularity of item - On a per-recommendation basis, bemorehuman allows you
to only show recommendations that are at a certain level of popularity. Sometimes people want
to see obscure things or sometimes we want to see popular things. It's very easy to tell
the recommendations generator "only give me recommendations of fairly obscure things."

- Recommendations using different scales - For a particular dataset, you can choose what the
ratings scale is. Maybe for one dataset you want to use a ratings scale of 1-5 because you
have "whole star ratings" where people can rate from 1-5 stars for things. Maybe for a
different dataset you want to be more expressive with your data and you want a 1-32 scale.
Using such a scale gives you more subtlety and gradations in the recommendations. It also
might help with the accuracy to give the algorithms more room to breathe, so to speak.

- Tested in real-world scenarios - Prior to open sourcing bemorehuman, it was used
commercially to recommend content across different datasets such as supermarket items, books,
beer, and movies. 

- Open source under a liberal license - bemorehuman is licensed under the MIT license which
allows for a wide range of usage in your public or private project. See the COPYING file
for more info.

- Reuse Software compliance - Reuse Software at https://reuse.software is an effort that allows
companies (or anyone, really) to understand more clearly how an open source project stacks
up from a licensing perspective. If a company is trying to figure out whether to
adopt an open source project internally, technical capability is often only one concern.
A big concern might be legal: do the lawyers give the thumbs-up for company adoption of the
OSS? Reuse Software compliance is something that helps legal teams do their analysis.
bemorehuman is fully Reuse Software compliant. That's important to us to help companies or
anyone else interested to adopt our software.

- No telemetry, no downloading of external models - bemorehuman does not "phone home." Nor 
does it download external models (or anything else) from the internet. Your data is your data 
and we have no interest in it. Additinoally, you might want to run bemorehuman disconnected 
from the internet, so we fully support that use case.

## Where can I find out more?

For help getting started with the code:

Keep reading this README file or browse the code itself. You can also ask questions on our GitHub
[Discussions](https://github.com/BeMoreHumanOrg/bemorehuman/discussions).

For motivations behind the source code:

See the [MANIFESTO](/MANIFESTO) file.

For custom support, development, or training:

Contact us at hi@bemorehuman.org or visit https://bemorehuman.org 

## Contributing

You can help by commenting on or addressing the [current issues](https://github.com/BeMoreHumanOrg/bemorehuman/issues).
Feature requests are welcome, too. Check out the [discussions](https://github.com/BeMoreHumanOrg/bemorehuman/discussions) and ask away.
See our [CONTRIBUTING](/CONTRIBUTING.md) file for more details.

To help foster a welcoming community, we've adopted the Contributor Covenant [Code of Conduct](/CODE_OF_CONDUCT.md).

## Licensing

bemorehuman is open source, released under the MIT license. See the COPYING file or
https://opensource.org/licenses/MIT for details. Contributions to this project are
accepted under the same or similar license. Individual files contain SPDX tags to
identify copyright and license information which allow this project to be machine-readable
for license details. bemorehuman is fully Reuse Software-compliant.

[![REUSE status](https://api.reuse.software/badge/github.com/BeMoreHumanOrg/bemorehuman)](https://api.reuse.software/info/github.com/BeMoreHumanOrg/bemorehuman)

[![CMake on multiple platforms](https://github.com/BeMoreHumanOrg/bemorehuman/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/BeMoreHumanOrg/bemorehuman/actions/workflows/cmake-multi-platform.yml)

## Installation instructions for bemorehuman

Steps involved:

1. Download bemorehuman.
2. Build the binaries.
3. Make working directory.
4. Integrate with a webserver.
5. (optional but recommended installation verification) Download and prep Grouplens movie rating dataset.
6. (optional but recommended installation verification) Run the test-accuracy binary and compare the
results against a known working system.


Detailed instructions:

### STEP 1: Download bemorehuman.

You can get bemorehuman at https://github.com/BeMoreHumanOrg/bemorehuman

For these instructions, let's say the source you clone or download/unpack resides at
~/src/bemorehuman

### STEP 2: Build the binaries.

#### Background info

- Bemorehuman doesn't deal with databases. We only deal with text files. This way we have zero database
dependencies. That said, I've tried to allow the flat files bemorehuman generates to be imported into
a database easily if you like.

#### Build Dependencies
- for test-accuracy:
  - openssl dev package
    - on Debian or Ubuntu, it's libssl-dev
    - on FreeBSD, it's openssl
    - on Void Linux, it's openssl-devel
    - on Mac, it's openssl installed like: "brew install openssl"
- (OPTIONAL) A webserver with fcgi capability. I like nginx for the simple fact it is commonly used and has an
excellent event model. Bemorehuman includes its own web server suitable for dev/test.
  - FastCGI library (only if you need to integrate with your own web server):
    - on Debian or Ubuntu, install libfcgi-dev 
    - on Void Linux, install fcgi-devel 
    - on NetBSD, install www/fcgi
    - NOTE: please add -DUSE_FCGI=ON to the first cmake command below
- (OPTIONAL) If you want to use protobuf instead of the default json: Protobuf-c:
  - on Debian or Ubuntu, "sudo apt install libprotobuf-c-dev protobuf-c-compiler"
  - on Void Linux, install protobuf-c-devel
  - on NetBSD, install devel/protobuf-c
  - on FreeBSD, install protobuf-c
  - OR on any OS from github.com/protobuf-c
  - NOTE: please add -DUSE_PROTOBUF=ON to the first cmake command below

 
#### Directory structure

- bemorehuman
  - lib (shared library stuff like config file reader)
  - ratgen (ratings generator)
  - valgen (valence generator)
  - recgen (runtime recommendation server)
  - test-accuracy (client-side commandline tester)
  - config (bemorehuman config file)

#### To build bemorehuman

Follow the regular cmake way of building:

     cd ~/src/bemorehuman    # or wherever the bemorehuman source is on your machine
     mkdir build; cd build
     cmake ..
     cmake --build .   
     sudo make install       # root is used to install the include/library/binaries and create config file under /etc)
    
     # if you are using your own HTTP server such as nginx, add "-DUSE_FCGI=ON" to the first cmake above 
     cmake -DUSE_FCGI=ON ..  
     # if you want to use protobuf instead of the default json, add "-DUSE_PROTOBUF=ON" to the first cmake above
     cmake -DUSE_PROTOBUF=ON ..  

Binaries built:
These are the binaries that get created automatically in the above cmake process:

- bmhlib (shared library)
- ratgen
- hum (our custom tiny HTTP server)
- valgen
- recgen
- test-accuracy

#### Additional build notes

- There's a working directory bemorehuman uses to store interim and output files. Default is /opt/bemorehuman
  - That's where you place a ratings file if you have one.
- bemorehuman-generated data is stored in .out files.
- Edit the config file /etc/bemorehuman.conf if you want different working dirs.
- Default config data supplied as input is in the BE environment in code.

### STEP 3: Make working directory.

    sudo mkdir /opt/bemorehuman
    sudo chown <user who'll run bemorehuman> /opt/bemorehuman

### STEP 4 (Optional): Integrate with your own webserver.

By default, bemorehuman uses its own HTTP server called hum. Hum caters to recgen's needs. Meaning, it 
handles POST requests from HTTP clients, but not GET. For this and the fact that there's very little 
error processing, please don't use hum in production. The instructions in this step are only for the
situation where you want to use your own webserver. If you're ok with using hum, please skip to Step 5.

If you want to use your own webserver, just make sure it can integrate with FastCGI. I like nginx. 
To install nginx from Debian or Ubuntu, "sudo apt install nginx"

The default socket used for communication between recgen and the webserver is

/tmp/bemorehuman/recgen.sock

So you'll need to inform your external webserver of that. For nginx, you can add this clause
to the server section of /etc/nginx/nginx.conf or /etc/nginx/sites-enabled/default:

    listen 8888
    location ^~ /bmh {
        include /etc/nginx/fastcgi_params;
        fastcgi_pass  unix:/tmp/bemorehuman/recgen.sock;
        fastcgi_keep_conn on;
    }

After making the above changes, restart your webserver. On Debian, it's:

    sudo service nginx restart

At this point bemorehuman is ready to use. However, I recommend you complete the following steps
to verify the installation and make sure recommendations are working correctly.

The idea is that when you have bemorehuman installed you can test with a known
dataset and check that your results are in line with what we got here at Be More
Human HQ.


### STEP 5 (Optional but recommended): Download and prepare Grouplens/Movielens movie rating dataset.

NOTE: All times below are from an Intel i7 8559u NUC development machine running Debian
Linux, with 20 GB RAM and an SSD drive.

NOTE: "Grouplens" is the name of a University of Minnesota research lab, "Movielens" is the name of the
movie ratings project. I use the terms interchangeably. They've been providing movie ratings data for
research purposes for a long time. I have no affiliation with either Grouplens nor the university.

The web page that describes the dataset:
https://grouplens.org/datasets/movielens/

The rest of these instructions assume you download the 25 million rating dataset. The actual file
to download:

https://files.grouplens.org/datasets/movielens/ml-25m.zip

To prepare the Movielens data:

- Unzip the data file to any local directory you like such as ~/data
- Copy ~/src/bemorehuman/valgen/convert_to_ints.sh to the directory where the just-unpacked ratings.csv is.

      cd ~/data/ml-25m
      cp ~/src/bemorehuman/valgen/convert_to_ints.sh .

- Run the valgen script "convert_to_ints.sh" in the Movielens data dir

      ./convert_to_ints.sh                    # takes about 10 seconds
    
- Copy the new Grouplens ratings data file ratings.out and movie file movies.csv to the bemorehuman working directory. 
This dir is specified as "working_dir" in the /etc/bemorehuman/bemorehuman.conf file. Default is /opt/bemorehuman

      cp ratings.out /opt/bemorehuman
      cp movies.csv /opt/bemorehuman

- Copy and run "normalize.sh" in /opt/bemorehuman. This script will ensure the movie ids are in sequence,
  which is important. This script also creates a little translation file so you can match original Grouplens
  id's to new bemorehuman id's. This file is called bmhid-glid.out

      cd /opt/bemorehuman
      cp ~/src/bemorehuman/valgen/normalize.sh .
      ./normalize.sh                          # takes about a minute  

- Now the Movielens data is ready to be run through bemorehuman.


### STEP 6 (Optional but recommended): Run the test-accuracy binary and compare the results against a known working system.

#### To run bemorehuman

- (If you want to use your own HTTP server) Make sure nginx or other webserver with FastCGI is running.
- Now you're ready to run the bemorehuman script. It's a simple wrapper that does the following:
  - create valences
  - start the live recommender
  - test your installation with the GroupLens data
- Given that you've followed Step 5 above, you invoke bemorehuman like so:

      ~/src/bemorehuman/bemorehuman -s 10 -t 20      # takes about 4 minutes to complete

- NOTE: If you're on Linux and bemorehuman complains about not finding libraries, you might need to run "sudo ldconfig" to
enable the new libraries you just built to be visible to bemorehuman.

- There are more ways to run the bemorehuman script (and its components) depending on your situation. See the
bemorehuman script and source files for more details.

- When you see "sleep 30" in the output, you're done! By default, the script will check every 30 seconds for new event data. You
can safely press "Control-C" to stop the bemorehuman script at this point and check the results as mentioned below.

Expected Results:

Please use the following numbers as a ballpark guide. All numbers below are from a desktop development machine.

- In terms of speed, we see around "974 micros per rec" for the time to generate recommendations,
but of course this is heavily dependent on your machine load and the random test users chosen at runtime.

- In terms of accuracy, here is some representative output:

"**Across all 10 users we're evaluating, Mean Absolute Error (MAE) is 1.895062, or 18.950617 percent for the ones we held back
Across all 10 users we're evaluating, random recs have MAE of 3.197531."

So you should expect to see similar results. Why similar and not exact? Because of the testing method. Users
evaluated for testing are chosen at random. We try to predict what we know they've already rated and the MAE is
how far away the prediction is relative to scale. In the above example MAE of 1.89 means we were just under 2
away on average, on a 10-scale. So that translates to just under 1 away on a 5-scale like what might be typical
when you ask people to rate things as one to five stars.

In our testing, these kinds of numbers are what we see on average no matter how many people are chosen at
random from the test-accuracy client. Your results with this dataset should be similar.

If your results are significantly different, such as an MAE of 2.7 or more when running "bemorehuman -s 10 -t 20" with
this Movielens dataset, then something's not right. In this situation, you may want to erase the contents of your
working directory (by default it's /opt/bemorehuman) and start again after the download part in step 5.

Enjoy!

If you have questions or comments, hit us up on GitHub at https://github.com/BeMoreHumanOrg or https://bemorehuman.org 


## bemorehuman recommendation engine concepts

### Big picture

For a list of background ideas and principles behind the technology, please read the MANIFESTO file.

### Definitions

ratgen: Ratings generator. If the input behaviour data is only things like purchase data or listen data then ratgen
  can be used to generate ratings which can then be fed into valgen.

valence: A pairwise relationship between two things that are rated. Valences get loaded into RAM in order to generate
  recommendations.

valgen: Valence generator. Inputs are ratings. Outputs are valences in a valences.out flat file.

recgen: Recommendation generator. Inputs are valences. Outputs are runtime recommendations. See test-accuracy for
  sample client implementation.

### Valgen pipeline

ratings in flat file --> "valgen" --> valences.out flat file

- The ratings file should have each line as personid, elementid, rating where
  - personid is 1..number of people
  - element id is 1..number of elements
  - rating is 1..5
- The valgen binary creates valences from ratings that are stored in ratings flat file.

### Recgen pipeline

- recgen pipeline:
  - valences.out output from valgen --> "recgen --valence-cache-gen" --> 2 binary valence cache files written to
  filesystem 
  - valences.out output from valgen --> "recgen --valence-cache-ds-only-gen" --> 2 DS binary valence cache files
  written to filesystem
