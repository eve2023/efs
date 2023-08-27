# EFS: Eve's Fruit Server

## Introduction

This C Webserver powers Eve's Fruitland website. It supports serving static files and parsing multipart/form-data for file uploading.

This product includes software developed by the Tcl project. View license terms: http://34.118.241.22/license.terms.txt

## Environment

Development:

* OS: Fedora 37
* Compiler: gcc 12.3

Production:

* OS: CentOS
* Compiler: gcc <??>

## Build

   gcc evesfruitland2.c efs3.c vn.c -o evesfruitland2 -ltcl

By default the server listens to port 8080. To build it to run on port <PORT>:

   gcc evesfruitland2.c efs3.c vn.c -o evesfruitland2 -ltcl -D PORT=<PORT>

## Usage

   ./evesfruitland2

To run in background:

   nohup ./evesfruitland2 > log.txt 2>&1 &

## License

This project is released under the GNU General Public License version 3 (GPLv3). For more information, see <https://www.gnu.org/licenses/gpl-3.0.en.html>.

By downloading, using, modifying, or distributing this code, you agree to the terms of the GPLv3 license.
