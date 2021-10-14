for /f %%p in (all_proto.txt) DO (
..\..\tool\protoc.exe -I.\proto %%p --cpp_out=.\message\
@if errorlevel 1 pause 
)
@echo *********success*********
@pause