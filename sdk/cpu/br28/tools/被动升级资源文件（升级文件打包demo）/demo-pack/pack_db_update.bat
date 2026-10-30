@echo off

@echo ********************************************************************************
@echo 			SDK BR28			
@echo ********************************************************************************
@echo %date%

cd %~dp0



packres.exe -n pupdate -o db_update.bin -normal -aligned 1 db_app.bin::0x01 0 TEST1::0x32 0 TEST2::0x32 0 new_res.bin::0x72 0



ping /n 2 127.1>null
IF EXIST null del null
pause
