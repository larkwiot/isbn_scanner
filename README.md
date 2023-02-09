# ISBN Scanner
Tool to find ISBNs in ebook files and fetch metadata from online databases

## Features

* WorldCat
* Tika
* JSON output
* Multi-threaded

# Installation

Make sure you have CMake and Ninja installed.

## Binary Release

```shell
yay -S libpugixml-dev mold
```

## Compiling from Source

```shell
cmake -S . --preset=Release
cmake --build ./out/build/Release -j<cores>
```

# Setup

## Tika

```shell
docker run -p 127.0.0.1:9998:9998 apache/tika:latest
```

# Usage

```shell
scanner -f filetypes.json -c scanner.toml -i <input directory> -o books.json
```

## Using the Results

[Recommend JQ](https://github.com/stedolan/jq)

## Roadmap

### v0.1
* [x] Better handling of the output JSON file
* [x] Add rate limiting

### v0.2
* [ ] Unit Tests
* [ ] Fuzz testing
* [ ] Changelog
* [ ] Complete README documentation

### v0.3
* [ ] Progress bar
* [ ] Improve fuzzy matching on filenames/titles
* [ ] Goodreads API
