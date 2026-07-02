# kew website

The website is built using [sblg](https://kristaps.bsd.lv/sblg/) and
[lowdown](https://kristaps.bsd.lv/lowdown/).  These tools are used to transform
the files [docs](docs) folder into a static website.

To build the site, type `make` this should generate html files. Type
`make install` to get a copy of the website in a folder called `site`
Change to the page branch and put the contents of the folder `site`
in the top level directory of the page branch.
Commit the changes and push to the Codeberg repository.

The site should appear on Codeberg pages site after configuring it following:
https://docs.codeberg.org/codeberg-pages/
