@echo off
REM $PostgreSQL: pgsql/src/tools/msvc/pgflex.bat,v 1.6 2009/11/12 00:13:00 tgl Exp $

IF NOT EXIST src\tools\msvc\buildenv.pl goto nobuildenv
perl -e "require 'src/tools/msvc/buildenv.pl'; while(($k,$v) = each %ENV) { print qq[\@SET $k=$v\n]; }" > bldenv.bat
CALL bldenv.bat
del bldenv.bat
:nobuildenv 

flex -V > NUL
if errorlevel 1 goto noflex

if "%1" == "src\backend\parser\scan.l" call :generate %1 src\backend\parser\scan.c -CF
if "%1" == "src\backend\bootstrap\bootscanner.l" call :generate %1 src\backend\bootstrap\bootscanner.c
if "%1" == "src\backend\utils\misc\guc-file.l" call :generate %1 src\backend\utils\misc\guc-file.c
if "%1" == "src\interfaces\ecpg\preproc\pgc.l" call :generate %1 src\interfaces\ecpg\preproc\pgc.c
if "%1" == "src\bin\psql\psqlscan.l" call :generate %1 src\bin\psql\psqlscan.c
if "%1" == "contrib\cube\cubescan.l" call :generate %1 contrib\cube\cubescan.c
if "%1" == "contrib\seg\segscan.l" call :generate %1 contrib\seg\segscan.c

echo Unknown flex input: %1
exit 1

:generate
flex %3 -o%2 %1
exit %errorlevel%

:noflex
echo WARNING! flex install not found, attempting to build without
exit 0
