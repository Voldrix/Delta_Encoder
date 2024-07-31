# Text Delta Encoder

#### Create a patch file that stores only the changes to a modified file, to re-create it from the source doc.

This algorithm was tuned for subtitle files, but works with any text document.\

### Usage

__Encode:__\
Patch file will turn src into modified file. Modified file can be discarded.\
`./delta_encode <src file> <modified file> <patch file>`\
Patch file must be saved disc, instead of printed to stdout, because it contains binary data.

__Decode:__\
Re-creates previously modified file from unmodified src and patch.\
`./delta_decode <src file> <.dlta patch file> [save filename]`\
Save to disk or print to stdout.

### How it Works
The tricky part is determining where an edit ends, since an edit can contain some of the same words, and further in the document beyond the edit could also contain a repeat statement. So how do we know where to reconnect?

Using two pointers you scan the entire rest of the document then increment the first pointer and do it again (On^2). Keep track of the rejoin point with the smallest sum of the two pointers (their offset from the start of the edit). Every time you encounter a new rejoin, only track it if it has a lower sum of the pointer offsets. Once you are done scanning, that is your rejoin point.

You can cut down on the time complexity by stopping the scan early. You can know for sure you've already found the earliest rejoin point as soon as you find one where the first pointer is greater than or equel to the minimum sum rejoin point.

#### Building
No prerequisites, just standard library.\
Just run make

#### License
[MIT](LICENSE.txt)
