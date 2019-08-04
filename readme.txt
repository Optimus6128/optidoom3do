
Here is the source code of OptiDoom as I was working with it.

For V0.2 I have automated a bit the process for someone wanting to compile and run it directly.
You only need a commercial ISO of the original game (which for obvious reasons I don't provide).

I've used some tools that support command line options for this process. I've removed OperaFS from the process since it didn't seem to provide command line, only GUI. I've also removed autohotkey script files, we don't need them anymore.

The new tools are:
* OperaTool by Cristina Ramos
  This is needed to extract commercial Doom ISO to folder, when running the batch fil in ISOdecompile (copy your ISO as doom.iso inside the folder first)

* 3doiso by nikk
  Compiles back folder to ISO. While OperaTool had a similar function, that had some issues, coming with an ISO that just fails to boot even if it's later encrypted.
* 3doEncrypt by Charles Doty
  This is what you need to do after compiling folder to Opera file system ISO. Encrypt the ISO in a way that it will boot without problems in real 3DO (and also emulators who don't skip that encryption)

First thing you have to do when you check out this project, would be to find a commercial Doom ISO (or extract it directly from a CD if you happen to own the original 3DO game in physical form, I guess using OperaFS which I don't provide here) and copy it inside the folder ISOdecompile as doom.iso. Then run the batch file and hopefully everything will go fine and a new folder named CD will be created outside. I will copy some additional files from CDextra like the new BannerScreen, boot_code file (these two are needed for the encryption to work, especially this boot_code file (the one already inside commercial Doom doesn't work, so I got this from a homebrew tutorial CD files), don't know why) and any new additional data files I might want to add in the future (like the new PSX aiff effects for v0.2)

Then you can go through the whole compile process, e.g. run the makeAndSign.bat to compile (you need some old version of ARM SDT, you could check some 3DO homebrew forums or ask me for help) and also do the whole process of copying the build LaunchMe executable to CD folder, then build the Opera file system ISO from CD folder, then encrypt it. An optidoom.iso will be created at the root.

I am trying to automate the process every time and make it easier for anyone else who would be curious to try compile the code and run or can't wait for the official release and wants to try it whenever I post updates.

Official OptiDoom page: http://bugothecat.net/releases/3DO/optidoom/optidoom_main.html
Github page: https://github.com/Optimus6128/optidoom3do

Bugo The Cat

optimus6128@yahoo.gr

youtube: https://www.youtube.com/user/Optimus6128
twitter: https://twitter.com/optimus6128