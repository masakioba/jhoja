# jhoja
jhoja is the Java disassembler for Jasmin

## What is the jhoja
jhoja is a disassembler of Java class file.  
Convert Java class file to Jasmin assembler code.  
jhoja can insert decompile protection code　for JD.(-g option)  
command line tool.  
does not have GUI.  
SWAG javap2 Ver 1.02 based.  
forked from SWAG javap2.  http://www.swag.uwaterloo.ca/javap2/

## Important problems
Re-assemble support Ascii code sources only.  
(without comments.)  
not support other character codes.  
jhoja support output UTF-8 strings.  
but Jasmin does not support UTF-8.  
if convert UTF-8 source to your local character code, may work on your locale systems.  
but it will be lost cross platforms.  
Decompile protection function support JD-GUI Decompiler.  
but does not support Jad Decompiler.  

## What is the difference between jhoja and javap,SWAG javap2?
jhoja support Jasmin assembler format code.  
and jhoja support decompile protection function.  
Oracle javap and SWAG javap2 does not support them.  
jhoja output UTF-8 code.  
javap2 does not support UTF-8.  

## Decompile protection function
Decompile protection function support JD-GUI Decompiler.  
but does not support Jad Decompiler.  
Decompile protection function does not affect the speed.  
but larger than normal class file.  
because insert protection code.  
require Jasmin(Java assembler) and JRE or JDK.  
not hard protection.  
if use jhoja and Jasmin then easily crack decompile protection.  

## Debug information
You can read any debug information in source comments.  
but re-assemble will be lost debug information in class file.  

## Support OS
Binary file support Microsoft Windows.  
Compiled by C++ Builder XE8.  
Written by C++. but allmost C.  
if recompile may work on other OS.  
jhoja for Windows work on GNU/Linux for Intel/AMD CPU with wine.  

## License
Apache v2 license  
(follow the SWAG's license.)  

## jhoja Source patch License
Apache v2 license  
(follow the SWAG's license.)  
(C) Masaki Oba  
admin@nabeta.tk  
http://www.nabeta.tk/en/jhoja/  
http://www.nabeta.tk/en/  
http://www.nabeta.tk/  

## Latest version
jhoja Ver 1.01

## Usage
Usage: jhoja [-g] [-h] xxx.class > xxx.j  
-g: Insert decompile protection code.  
-h: show help. 
  
http://www.nabeta.tk/en/jhoja/
