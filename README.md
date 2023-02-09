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

* [x] Better handling of the output JSON file
* [x] Add rate limiting
* [ ] Unit Tests
* [ ] Fuzz testing
* [ ] Better handling when there are multiple ISBNs (fuzzy matching on filename)
* [ ] Goodreads API
