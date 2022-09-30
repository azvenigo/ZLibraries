# ZZip
A cross platform tool for creating, exploring, diffing, extracting and updating local files from ZIP files both local and on remote HTTP servers.


# Latest Updates
* As of 2022/09/29 The tool has been migrated from a stand-alone ZZipUpdate project. 
* I've also migrated from boost to using libcurl to support additional protocols.
* As boost filesystem library has been added to standard libraries I've moved to that and removed boost entirely.


# Requirements



# Main Features

* Cross Platform
  * Uses libcurl for networking.
  * Uses OpenSSL for TLS support.
* ZIP file support
  * ZIP64 support (for very large zip archives.)
  * Deflate support
* Simple use and simple code to use in your own projects. 
  * Perform in-memory or disk to disk compression/decompression.
* Secure and flexible
  * Supports HTTP and HTTPS connections.
* Very fast
  * Multithreaded, job system based.
  * Uses FastCRC for very fast CRC calculation.


# Multithreaded

  The utility performs multithreaded extraction/updating/verification for taking advantage of SSDs having no seek times making operations extremely fast.

  For example:
    Test ZIP file of 1.68GB containing 499 folders and 3962 files compressed to 58% of normal size.

    Extraction time: 7-Zip  takes 2m 13s
                     ZZip   takes 0m 19s
            
    Once extracted, to do another update (verifying CRCs of all files) takes ZZip only 1.7s

# Remote Zip File Access
  
You can extract one, all or some wildcard matching set of files from a remotely hosted zip file. You don't have to download the entire zip file first. So for particularly large zip archives this is a very fast and convenient way of listing or extracting files.
  
# Update

The utility can compare the contents of a locally extracted subtree of files to a zip file (either local or remote) and extract only the files that are different. This can be used for a very fast and easy way of keeping your local files in sync with a published zip archive. Only the ZIP central directory is used for doing the file comparisons so in cases where no files have changed the entire operation can complete in milliseconds. An application could use this functionality on startup to perform a quick self-update.
Update can also be used as a form of "repair" for an application in that any locally modified, incomplete or corrupt files can be fixed.
  
# Diff

  The utility can diff files in a local subtree and zip file and report what's different. Which files/folders exist only on the disk vs which ones are only in the ZIP archive and which are different.
  The reports can be output in tab, commas or HTML for easy reading or use by other applications.

# Examine
  The utility can list the contents of ZIP archives without downloading them.

# Asset Update
  You can embed the code in your own application to perform updates of assets at any time, on startup or on demand.
  
# Self Update
  The utility can be used for your application to do a fast self update. See "Usage" for details.

# Work in Progress
 It's a work in progres so there are still features I would love to add or see added.
 -Unicode support
 -Additional compression methods
 -Support for killing applications holding files open during extraction

# Usage

# ZZip.exe Usage Examples

The following will download any files missing or different from c:/example that are in the package:
    
    ZZip.exe sync https://www.mysecuresite.com/latest/files.zip c:/example

The following will extract all jpg files with 8 threads:
    
    ZZip.exe extract d:/downloads/pictures.zip c:/albums *.jpg -threads:8 

The following will report differences between a path and a package and create an HTML report called results.html:

    ZZip.exe diff http://www.mysite.com/game_1.5.2.zip "c:/Program Files (x86)/Game/" -outputformat:html > results.html

The following will list the contents of a zip file on a server in a comma delimited format

    ZZip.exe list https://www.mysite.com/sample.zip -outputformat:commas

The following will create a new zip archive and all JPG files that contain "Maui" in the specified path:

    ZZip.exe create c:/temp/mytrip.zip "f:\My Albums\2019\Hawaii Trip\" *Maui*.jpg


# Code Usage Examples

There are several ways in which you can use these classes.
At the highest level you can instantiate a ZZipJob object, configure it and let it run. Querying it for status or calling Join when you want to wait for it to complete.

    ZipJob newJob(ZipJob::kExtract);
    newJob.SetBaseFolder(L"c:/output");
    newJob.SetURL(L"c:/downloads/files.zip");
    newJob.Run();
    newJob.Join();


You can also use ZZipAPI directly. For example you can retrieve a file from a zip archive on a server directly into memory:

    ZZipAPI zipAPI;
    zipAPI.Init(L"http://www.sample.net/files.zip");
    zipAPI.DecompressToBuffer("readme.txt", pOutputBuffer);





## Self Update
Building a self update feature into an application can time consuming and error prone. The simplest approach is to have an application check a server for a newer version; and if available to download a new setup package and run it. While this is safe this can take significantly longer than necessary for your user. 

Instead consider using ZZip's methods:
### Method 1
1. Publish your application's installer and a zip version of the application next to it. For example:
     * "https://myserver/app_installer.exe"
     * "https://myserver/app_package.zip"
2. Add a launcher to your application to call ZZip.exe for self update.
     * Whenever your application is launched call ZZip.exe with your zip package and your install path.
     * Example command line for self update:


`     ZZip.exe update https://myserver/app_package.zip "c:\Program Files\My Application"`


ZZip will extract the zip central directory and check all your local files for anything that isn't up to date or missing and download the minimum required files. Even on reasonably large applications this may only take a few seconds which is reasonable for startup times. If you wish you could also add a version check before launching ZZip to save the few seconds.

### Method 2
1. Follow method 1 but publish your application's installer as a self-extracting ZIP file.
     * "https://myserver/app_installer.exe" 
     * Example command line for self update:


`     ZZip.exe update https://myserver/app_package.exe "c:\Program Files\My Application"`


ZZip can work with self-extracting zip files and you can specify the EXE as a package.






