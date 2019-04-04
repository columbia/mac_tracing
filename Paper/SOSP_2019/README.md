### Step 1: Set up latex environment
```
sudo apt-get install texlive-full lgrind
```
Or in mac
```
brew cask install mactex
add the path /Library/TeX/texbin in /etc/paths


### Step 2: Build the project
The following command will create paper.pdf under the source code folder.

#### If you're using Linux, please run the following commands.
```
make clean all && make
```

#### If you're using Windows, please run the following commands.
```
pdflatex paper
bibtex paper
pdflatex paper
pdflatex paper
```
