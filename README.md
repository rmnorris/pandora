| Branch             | Status                                                                           |
|:-------------------|:---------------------------------------------------------------------------------|
| [`master`][master] | ![Travis (.com) branch](https://img.shields.io/travis/com/rmcolq/pandora/master) |
| [`dev`][dev]       | ![Travis (.com) branch](https://img.shields.io/travis/com/rmcolq/pandora/dev)    |

[master]: https://github.com/rmcolq/pandora/tree/master
[dev]: https://github.com/rmcolq/pandora/tree/dev


# Pandora

[TOC]: #

# Table of Contents
- [Introduction](#introduction)
- [Quick Start](#quick-start)
- [Hands-on toy example](#hands-on-toy-example)
- [Installation](#installation)
  - [Containers](#containers)
  - [Installation from source](#installation-from-source)
- [Usage](#usage)


## Introduction
Pandora is a tool for bacterial genome analysis using a pangenome reference graph (PanRG). It allows gene presence/absence detection and genotyping of SNPs, indels and longer variants in one or a number of samples. Pandora works with Illumina or Nanopore data. For more details, see [our paper][pandora_2020_paper].

The PanRG is a collection of 'floating'
local graphs (PRGs), each representing some orthologous region of interest
(e.g. genes, mobile elements or intergenic regions). See
https://github.com/rmcolq/make_prg for a pipeline which can construct
these PanRGs from a set of aligned sequence files.

Pandora can do the following for a single sample (read dataset):
- Output inferred mosaic of reference sequences for loci (eg genes) from the PRGs which are present in the PanRG;
- Output a VCF showing the variation found within these loci, with respect to any reference path in the PRGs;
- Discovery of new variation not in the PanRG.

For a collection of samples, it can:
- Output a matrix showing inferred presence-absence of each locus in each sample genome;
- Output a multisample pangenome VCF including genotype calls for each sample in each of the loci. Variation is shown with respect to the most informative recombinant path in the PRGs (see [our paper][pandora_2020_paper]).

> **Warning - `pandora` is not yet a production-ready tool.** 

## Quick Start

Index PanRG file:

```
pandora index -t 8 <panrg.fa>
```

Compare first 30X of each Illumina sample to get pangenome matrix and
VCF

```
pandora compare --genotype --illumina --max-covg 30 <panrg.fa> <read_index.tab>
```

Map Nanopore reads from a single sample to get approximate sequence for
genes present

```
pandora map <panrg.fa> <reads.fq>
```

## Hands-on toy example

You can test `pandora` on a toy example following [this link](example).
There is no need to have `pandora` installed, as it is run inside containers.

## Installation

### No installation needed - precompiled portable binary

You can use `pandora` with no installation at all by simply downloading the precompiled binary, and running it.
In this binary, all libraries are linked statically, except for OpenMP.

* **Requirements**
  * The only dependency required to run the precompiled binary is OpenMP 4.0+;
  * The easiest way to install OpenMP 4.0+ is to have GCC 4.9 (from April 22, 2014) or more recent installed, which supports OpenMP 4.0;
  * Technical details on why OpenMP can't be linked statically
can be found [here](https://gcc.gnu.org/onlinedocs/gfortran/OpenMP.html). 

* **Download**:
  ```
  wget "https://www.dropbox.com/s/ltq2gti9t6wav1j/pandora-linux-precompiled_v0.8.1_beta?dl=1" -O pandora-linux-precompiled_v0.8.1_beta
  ```
  * **TODO: updated to a github link when we make the release;**
* **Running**:
```
chmod +x pandora-linux-precompiled_v0.8.1_beta
./pandora-linux-precompiled_v0.8.1_beta -h
```

* **Compatibility**: This precompiled binary works on pretty much any glibc-2.12-or-later-based x86 and x86-64 Linux distribution 
  released since approx 2011. A non-exhaustive list: Debian >= 7, Ubuntu >= 10.10, Red Hat Enterprise Linux >= 6,
  CentOS >= 6;
  
* **Credits**:
  * Precompilation is done using [Holy Build Box](http://phusion.github.io/holy-build-box/);
  * We acknowledge Páll Melsted since we followed his [blog post](https://pmelsted.wordpress.com/2015/10/14/building-binaries-for-bioinformatics/) to build this portable binary.

* **Notes**:
  * We provide precompiled binaries for Linux OS only;
  * The performance of precompiled binaries is several times slower than a binary compiled from source.
    The main reason is that the precompiled binary can't contain specific instructions that might speed up
    the execution on specific processors, as it has to be runnable on a wide range of systems;

### Containers

![Docker Cloud Build Status](https://img.shields.io/docker/cloud/build/rmcolq/pandora)

You can also download a containerized image of Pandora.
Pandora is hosted on Dockerhub and images can be downloaded with the
command:

```
docker pull rmcolq/pandora:latest
```

Alternatively, using singularity:

```
singularity pull docker://rmcolq/pandora:latest
```

NB For consistency, we no longer maintain images on singularity hub.

### Installation from source

This is the hardest way to install `pandora`, but that yields the most optimised binary.

Requirements:
- A Unix or Mac OS, with a C++11 compiler toolset (e.g. `g++`, `ld`, `make`, `ctest`, etc), `cmake`, `git` and `wget`.

- Download and install `pandora` as follows:

```
git clone --single-branch https://github.com/rmcolq/pandora.git --recursive
cd pandora
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release .. 
make -j4
ctest -VV
```

* If you want to produce meaningful stack traces in case `pandora` errors out, `binutils-dev` must be installed and the
  `cmake` must receive this additional parameter: `-DPRINT_STACKTRACE=True`.

## Usage

See [Usage](https://github.com/rmcolq/pandora/wiki/Usage).


<!--Link References-->
[pandora_2020_paper]: https://www.biorxiv.org/content/10.1101/2020.11.12.380378v2
