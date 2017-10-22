# nzbtotar

An efficient Linux cli tool to download and extract NZB with RAR archives to a streaming TAR format.

Note that it does not touch disk and makes efficient use of CPU and memory, making it ideal for running this from Raspberry PI and similar embedded devices.

The TAR format can be reused using standard command-line tools to stream or watch videos.

## Requirements

* `libxml2`
* `vrb`
* `expat`

You can use `nix-shell` to get an appropriate development environment with all requirements installed.

## Building

```
make
```

## Usage

```
./nzbtotar --hostname HOSTNAME --port PORT --username USERNAME --password PASSWORD NZBFILE
```

Use the `HOSTNAME`, `PORT`, `USERNAME` and `PASSWORD` of your usenet provider. Note that `nzbtotar` does not support secured connections, so use an unsecured port.

Use a local path to a file for `NZBFILE`. Note that the NZB file must contain RAR archives in the form of `.rXX` or `.part01.rar`. Most NZB files online use this format.

For example:

```
./nzbtotar --hostname your.usenet.com --port 119 --username your-username --password your-secret-password /tmp/your.nzb.file.to.download.and.extract.nzb
```

`nzbtotar` will output a tar stream from stdout.

## Examples

```
./nzbtotar --hostname HOSTNAME --port PORT --username USERNAME --password PASSWORD NZBFILE | tar x -C /tmp/
```

Will download and extract the RAR contents of the NZB to `/tmp/` on disk.


```
./nzbtotar --hostname HOSTNAME --port PORT --username USERNAME --password PASSWORD NZBFILE | tar -xO --wildcards --ignore-case '*.mp4' '*.mkv' '*.avi' > /tmp/video.mp4
```

Will download and extract a single video file from the RAR contents of the NZB to `/tmp/video.mp4` on disk.

```
./nzbtotar --hostname HOSTNAME --port PORT --username USERNAME --password PASSWORD NZBFILE | tar -xO --wildcards --ignore-case '*.mp4' '*.mkv' '*.avi' | mplayer -
```

Will stream a single video file from the NZBFILE to mplayer, allowing you to instantly watch videos.

```
./nzbtotar --hostname HOSTNAME --port PORT --username USERNAME --password PASSWORD NZBFILE | tar -xO --wildcards --ignore-case '*.mp4' '*.mkv' '*.avi' | ffmpeg -i - -vcodec libx264 -preset ultrafast -tune zerolatency -s 800x480 -f mpegts - > /tmp/video.mp4
```

Will re-encode a video in the NZB and store the result in `/tmp/video.mp4`.
