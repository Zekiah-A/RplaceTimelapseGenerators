if [ ! -d "./rslashplace2.github.io" ]; then
    git clone https://github.com/rslashplace2/rslashplace2.github.io
fi
cd ./rslashplace2.github.io
git log --pretty=format:"Commit: %H%nAuthor: %an%nDate: %cd" --date=format:"%s" > ../repo_commit_hashes.txt
cd ..

if [ ! -d "./canvas1" ]; then
    git clone https://github.com/rplacetk/canvas1
fi
cd ./canvas1
git log --pretty=format:"Commit: %H%nAuthor: %an%nDate: %cd" --date=format:"%s" > ../canvas_1_commit_hashes.txt
cd ..