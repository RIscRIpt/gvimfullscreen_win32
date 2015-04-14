gvimfullscreen.dll: gvimfullscreen.c
	cl /MD /LD user32.lib gdi32.lib gvimfullscreen.c

clean:
	del *.obj
	del *.dll
	del *.exp
	del *.lib
