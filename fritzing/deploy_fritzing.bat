cd /d %~dp0

set svndir="C:\Program Files\SlikSvn\bin\"
%svndir%svn export https://fritzing.googlecode.com/svn/trunk/fritzing/translations ./release/translations

%svndir%svn export https://fritzing.googlecode.com/svn/trunk/fritzing/bins ./release/bins

%svndir%svn export https://fritzing.googlecode.com/svn/trunk/fritzing/sketches ./release/sketches

%svndir%svn export https://fritzing.googlecode.com/svn/trunk/fritzing/parts ./release/parts

%svndir%svn export https://fritzing.googlecode.com/svn/trunk/fritzing/pdb ./release/pdb
%svndir%svn export https://fritzing.googlecode.com/svn/trunk/fritzing/help ./release/help



del .\release\translations\*.ts
cd release
cd translations
for /f "usebackq delims=;" %%A in (`dir /b *.qm`) do If %%~zA LSS 1024 del "%%A"
cd ..
cd ..