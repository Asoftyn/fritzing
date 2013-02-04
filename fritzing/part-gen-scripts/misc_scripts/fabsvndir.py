

import getopt, sys, os, os.path, re, zipfile, shutil
import xml.dom.minidom
import subprocess

def usage():
    print """
usage:
    fabsvndir.py -f [orders path] -s [start index] -e [end index]
    create new fab folders
    """
    
           
def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hf:s:e:", ["help", "from", "start", "end"])
    except getopt.GetoptError, err:
        # print help information and exit:
        print str(err) # will print something like "option -a not recognized"
        usage()
        return
    fromName = None
    start = None
    end = None
    
    for o, a in opts:
        #print o
        #print a
        if o in ("-f", "--from"):
            fromName = a
        elif o in ("-s", "--start"):
            try:
                start = int(a)
            except:
                print "start is not an int", a
                return
        elif o in ("-e", "--end"):
            try:
                end = int(a)
            except:
                print "end is not an int", a
                return
        elif o in ("-h", "--help"):
            usage()
            return
        else:
            assert False, "unhandled option"
    
    if not(fromName):
        usage()
        return
        
    if not(start):
        usage()
        return
        
    if not(end):
        usage()
        return
        
    if end < start:
        print "end must be greater than start"
        usage()
        return
        
    if not os.path.exists(fromName):
        print fromName, "doesn't exist"
        return
        
    if not os.path.isdir(fromName):
        print fromName, "isn't a folder"
        return
        
    for ix in range(end - start + 1):
        newPath = os.path.join(fromName, str(ix + start).zfill(6))
        try:
            os.mkdir(newPath)
        except:
            print "error", sys.exc_info()[0]
        if os.path.isdir(newPath):
            subprocess.call(["svn", "add", newPath])
            print "adding dir", newPath
        else:
            print "unable to add dir", newPath
        
    subprocess.call(["svn", "commit", "-m", "add new folders", fromName])

if __name__ == "__main__":
    main()



