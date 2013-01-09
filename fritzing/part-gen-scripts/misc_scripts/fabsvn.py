

import getopt, sys, os, os.path, re, zipfile, shutil
import xml.dom.minidom
import subprocess

def usage():
    print """
usage:
    fabsvn.py -f [panelizer xml path] -c
    checks in the fzz, svn, and gerber files for a fab round.  If -c is included, will check in the custom files as well.
    """
    
           
def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hf:c", ["help", "from", "custom"])
    except getopt.GetoptError, err:
        # print help information and exit:
        print str(err) # will print something like "option -a not recognized"
        usage()
        sys.exit(2)
    fromName = None
    custom = None
    
    for o, a in opts:
        #print o
        #print a
        if o in ("-f", "--from"):
            fromName = a
        elif o in ("-h", "--help"):
            usage()
            sys.exit(2)
        elif o in ("-c", "--custom"):
            custom = True
        else:
            assert False, "unhandled option"
    
    if(not(fromName)):
        usage()
        sys.exit(2)
        
    inputdir = os.path.dirname(fromName)
    if len(inputdir) == 0:
        print "no input folder found"
        return
           
    try:
        dom = xml.dom.minidom.parse(fromName)
    except xml.parsers.expat.ExpatError, err:
        print "xml error ", str(err), "in", fromName
        return 
        
    root = dom.documentElement
        
    boardsNodes = root.getElementsByTagName("boards")
    boards = None
    if (boardsNodes.length == 1):
        boards = boardsNodes.item(0)                
    else:
        print "more or less than one 'boards' element in", fromName
        return
        
    boardNodes = boards.getElementsByTagName("board")
    filenames = []
    filenames.append(fromName)
    for board in boardNodes:
        originalName = board.getAttribute("name")
        if originalName == None:
            continue
        
        if len(originalName) == 0:
            continue
            
        path = os.path.join(inputdir, originalName)
        if os.path.isfile(path):
            filenames.append(path)
    
    for path in filenames:
        subprocess.call(["svn", "add", path])

    gerberpath = os.path.join(inputdir, "gerber")
    if os.path.isdir(gerberpath):
        subprocess.call(["svn", "add", gerberpath])
        
    svgpath = os.path.join(inputdir, "svg")
    if os.path.isdir(svgpath):
        subprocess.call(["svn", "add", "--non-recursive", svgpath])
        norotatepath = os.path.join(svgpath, "norotate")
        if os.path.isdir(norotatepath):
            subprocess.call(["svn", "add", norotatepath])
        for fn in os.listdir(svgpath):
            if fn.endswith("identification.pdf"):
                subprocess.call(["svn", "add", os.path.join(svgpath, fn)])
        
    if custom:
        custompath = os.path.join(inputdir, "custom")
        if os.path.isdir(custompath):
            subprocess.call(["svn", "add", "--non-recursive", custompath])
            customgerberpath = os.path.join(custompath, "gerber")
            if os.path.isdir(customgerberpath):
                subprocess.call(["svn", "add", customgerberpath])
            customsvgpath = os.path.join(custompath, "svg")
            if os.path.isdir(customsvgpath):
                subprocess.call(["svn", "add", "--non-recursive", customsvgpath])
                for fn in os.listdir(customsvgpath):
                    if fn.endswith("identification.pdf"):
                        subprocess.call(["svn", "add", os.path.join(customsvgpath, fn)])
                        
    subprocess.call(["svn", "commit", "-m", "fabsvn automated commit", inputdir])

if __name__ == "__main__":
    main()



