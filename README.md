# CK2toEU4
Converter for CK2 savegames to EU4.

User instructions are in documentation.pdf. This file is for people interested in
compiling the code themselves.

Dependencies are Qt (I use the ancient 2010.05 version; newer ones *should* work),
my ParadoxParser library which you can find here: 

https://github.com/kingofmen/ParadoxParser

The parser in turn depends on Boost; I use 1.41.0, which is older than the hills,
but newer versions *should* also work.

I generate makefiles for the converter thus:

c:\Qt\2010.05\qt\bin\qmake -project "CONFIG+=exceptions c++0x"
c:\Qt\2010.05\qt\bin\qmake -makefile -after \
  "LIBS+=C:\Users\MyUserName\Desktop\ParadoxParser\libParser.lib" \
  "INCLUDEPATH+=C:\Users\MyUserName\Desktop\ParadoxParser" \
  "INCLUDEPATH+=C:\Users\MyUserName\Desktop\boost_1_54_0" \
  "LIBS+=-static-libgcc" 
  
This assumes that you have Qt installed; adjust the paths according to taste. 
To compile, I do:

mingw-make -f Makefile.[Debug,Release]

You will likely need some tweaking to make it work on Linux, since I do use
some Windows-specific file munging; sorry about that. If you take the time to
do this, please let me know so I can try to include it.
