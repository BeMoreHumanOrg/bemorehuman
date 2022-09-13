<!---
SPDX-FileCopyrightText: 2022 Brian Calhoun <brian@bemorehuman.org>
SPDX-License-Identifier: MIT
-->

## What does bemorehuman do?

bemorehuman is a recommendation engine; sometimes these things are called recommender systems.
It recommends things such as books, or movies, or beer, or music bands to people. You input
implicit or explicit ratings data or purchase data or click data, and bemorehuman will build
a model of that dataset. With the model built and loaded into RAM, you can then query the
system via a REST interface and receive recommendation in real-time. 

## Why is it useful?

Recommending things to people is something that lots of companies have tried to do for many
years. Everyone from Amazon to Netflix to TikTok and even smaller content providers and
ecommerce stores want to recommend things to visitors and shoppers. These recommendations
can appear on a website, in email, in a feed, or really whereever a person interacts with
the company or brand.

## Where can I find out more?

For help getting started with the code:

Keep reading this README file or browse the code itself. You can also ask questions on our Github
discussions:

https://github.com/BeMoreHumanOrg/bemorehuman/discussions

For motivations behind the source code:

See the MANIFESTO file.

For custom support, development, or training:

Contact us at hi@bemorehuman.org or visit http://bemorehuman.org 

## Contributing

You can help by commenting on or addressing the [current issues](https://github.com/BeMoreHumanOrg/bemorehuman/issues)
Feature requests are welcome, too. Check out the [discussions](https://github.com/BeMoreHumanOrg/bemorehuman/discussions) and ask away.

## Licensing

bemorehuman is open source, released under the MIT license. See the LICENSE file or
https://opensource.org/licenses/MIT for details. Contributions to this project are
accepted under the same or similar license. Individual files contain SPDX tags to
identify copyright and license information which allow this project to be machine-readable
for license details. bemorehuman is fully Reuse Software-compliant.


## Installation instructions for bemorehuman

Steps involved:

1. Download bemorehuman.
2. Build the binaries.
3. Make working directory.
4. Integrate with a webserver.
5. (optional but recommended installation verification) Download and prep Grouplens movie rating dataset.
6. (optional but recommended installation verification) Run a test-accuracy binary and compare the
results against a known working system.


Detailed instructions:

### STEP 1: Download bemorehuman.

You can get bemorehuman at https://github.com/BeMoreHumanOrg/bemorehuman

For these instructions, let's say the source you clone or download/unpack resides at
~/src/bemorehuman

### STEP 2: Build the binaries.

#### Background info

- Bemorehuman doesn't deal with databases. Just flat files. This way we have zero database
dependencies. That said, I've tried to allow the flat files bemorehuman generates to be imported into
a database easily if you like.

#### Build Dependencies

- A webserver with fcgi capability. I like nginx for the simple fact it is commonly used and has a
smart event model.
  - see nginx.conf in bemorehuman/config dir for a sample
- You can get standard fcgi from lots of places; I used: https://github.com/toshic/libfcgi
  - on Debian or Ubuntu, install libfcgi-dev OR
  - on Void Linux, install fcgi-devel 
- An implementation of protobuf-c:
  from Debian or Ubuntu via "sudo apt install libprotobuf-c-dev protobuf-compiler" OR
  from Void Linux via "sudo xbps-install protobuf-c-devel" OR
  from github.com/protobuf-c

  If you don't have a recgen.proto in the bemorehuman/recgen dir (it should already be there)
  - modify recgen.proto as needed then generate the recgen .c and .h (recgen.pb-c.c/.h) on command line:
  protoc --c_out=. recgen.proto
- for test-accuracy:
  - curl
  - openssl dev package
    - on Debian or Ubuntu, it's libssl-dev

#### Directory structure

- bemorehuman
  - lib (shared library stuff like config file reader)
  - ratgen (ratings generator)
  - valgen (valence generator)
  - recgen (runtime recommendation server)
  - test-accuracy (client-side commandline tester)
    - testfiles (testing files used in conjunction with test-accuracy)
  - config (various config files)

#### To build bemorehuman

Run build.sh and remedy any "FAIL" messages you may need to. The build script:
- assumes you've downloaded and unpacked the source code to ~/src/bemorehuman
- will create a few "build-xxx" directories in your home dir, corresponding to the binaries to be built
  (needs work to be less manual)
  
More details:
We use cmake to build. These are the binaries to be built and they all have their own CMakeLists.txt:

bmhlib (shared library)
ratgen
valgen
recgen
test-accuracy

#### Additional build notes

- There's a working directory bemorehuman uses to store interim and output files. Default is /opt/bemorehuman
  - That's where you place a ratings file if you have one.
- bemorehuman-generated data is stored in .out files.
- Edit the config file /etc/bemorehuman.conf if you want different working dirs.
- Default config data supplied as input is in the BE environment in code.

### STEP 3: Make working directory.

sudo mkdir /opt/bemorehuman
sudo chown <user who'll run bemorehuman> /opt/bemorehuman

### STEP 4: Integrate with a webserver.

I like nginx, but any webserver with FastCGI will do. To install nginx from Debian or Ubuntu,
"sudo apt install nginx"

The default socket used for communication between recgen and the webserver is

/tmp/bemorehuman/recgen.sock

So you'll need to inform your webserver of that. For nginx, you can add this clause
to the server section of /etc/nginx/nginx.conf or /etc/nginx/sites-enabled/default:

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


### STEP 5 (optional but recommended): Download and prepare Grouplens/Movielens movie rating dataset.

NOTE: All times below are from an Intel i7 8559u NUC development machine running Debian
Linux, with 20GB RAM and an SSD drive.

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

    cd ~/data/movielens/ml-25m
    cp ~/src/bemorehuman/valgen/convert_to_ints.sh .

- Run the valgen script "convert_to_ints.sh" in the Movielens data dir

    ./convert_to_ints.sh                    # takes about 10 seconds
    
- Copy the new Grouplens ratings data file ratings.out to the bemorehuman working directory. This dir
is specified as "working_dir" in the /etc/bemorehuman/bemorehuman.conf file. Default is /opt/bemorehuman

    cp ratings.out /opt/bemorehuman

- Copy and run "normalize.sh" in /opt/bemorehuman. This script will ensure the movie ids are in sequence,
  which is important. This script also creates a little translation file so you can match original Grouplens
  id's to new bemorehuman id's. This file is called bmhid-glid.out

    cd /opt/bemorehuman
    cp ~/src/bemorehuman/valgen/normalize.sh .
    ./normalize.sh                          # takes about a minute  

- Now the Movielens data is ready to be run through bemorehuman.


### STEP 6 (optional but recommended): Run the test-accuracy binary and compare the results against a known working system.

#### To run bemorehuman

- Make sure nginx (or other webserver with FastCGI) is running
- Now you're ready to run the bemorehuman script. It's a simple wrapper that does the following:
  - create valences
  - start the live recommender
  - test your installation with the GroupLens data
- Given that you've followed Step 5 above, you invoke bemorehuman like so:

    ~/src/bemorehuman/bemorehuman exp10            # takes about 7 min

- There are more ways to run the bemorehuman script (and its components) epending on your situation. See the
bemorehuman script and source files for more details.

Expected Results:

Please use the following numbers as a ballpark guide. All numbers below are from a desktop development machine.

- In terms of speed, we see around "25 millis per person" for the time to generate recommendations,
but of course this is heavily dependent on your machine load and the random test users chosen at runtime.

- In terms of accuracy, here is some representative output:

"**Across all 10 users we're evaluating, Mean Absoluete Error (MAE) is 1.895062, or 18.950617 percent for the ones we held back
Across all 10 users we're evaluating, random recs have MAE of 3.197531."

So you should expect to see similar results. Why simliar and not exact? Because of the testing method. Users
evaluated for testing are chosen at random. We try to predict what we know they've already rated and the MAE is
how far away the prediction is relative to scale. In the above example MAE of 1.89 means we werea just under 2
away on average, on a 10-scale. So that translates to just under 1 away on a 5-scale like what might be typical
when you ask people to rate things as one to five stars.

In our testing, these kinds of numbers are what we see on average no matter how many people are chosen at
random from the test-accuracy client. Your results with this dataset should be similar.

If your results are significantly different, such as an MAE of 2.5 or more when running "bemorehuman exp10" with
this Movielens dataset, then something's not right. In this situation, you may want to erase the contents of your
working directory (by default it's /opt/bemorehuman) and start again after the download part in step 5.

Enjoy!

If you have questions or comments, hit us up on github at https://github.com/BeMoreHumanOrg or https://bemorehuman.org 


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
