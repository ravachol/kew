# getcover

## getcover - get cover art image from music files

getcover extracts a cover-art image from one of music files in the specified directory and creates an image file named ‘Folder.jpg’ in the same directory. If the image format in the music file is PNG then getcover generates a warning message and creates an image file named ‘Folder.png’.

Note: JPEG format is preferred to PNG format for music files because of smaller size.

 Supported Formats are:  
  FLAC: Free Lossless Audio Codec  
  ALAC: Apple Lossless Audio Codec (m4a)  
  AAC: Advanced Audio Coding (m4a)  

## Compile:
```
 $ make  
 $ sudo make install
```

## Usage:
```
 getcover [-v] [-o] [-f basename] path [path [path]...]
```
 *  -v: verbose mode  
 *  -o: override mode. override Folder.jpg even if it exists  
 *  -f basename: specifiy name of image file without suffix  
 *  path: path of the directory which contains music files  

 
## Example:

To extract a cover art from music files in the current directory:  
```
 $ getcover . 
```

Specifiy filename for cover art. Cover.jpg for JPEG data or Cover.png for PNG data.  
```
 $ getcover -f Cover .
```

To extract cover arts from music files in all directories immediately under the current directory:  
```
 $ getcover *
```

To extract cover arts from music files in all sub directories under the specified directory:  
```
 $ sudo find /var/lib/mpd/music -type d -exec getcover {} \;
```

