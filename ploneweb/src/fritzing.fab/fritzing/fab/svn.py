import sys, os, getopt, tempfile, subprocess, urllib, errno

    
def usage():
    print """
usage:
    svn.py -r <production-round> -o <order> -f <file-url> -u <username> -p <password> -s <svn-url>
    
    Commits a given Fritzing file to an SVN repository, into the folder for the given
    production round.
"""

def main():

    try:
        opts, args = getopt.getopt(sys.argv[1:], "hr:o:f:u:p:s:", ["help", 
            "production-round", "order", "file-url", "username", "password", "svn-url"])
    except getopt.GetoptError, err:
        # print help information and exit:
        print str(err) # will print something like "option -a not recognized"
        usage()
        sys.exit(2)

    pround = None
    order = None
    file_url = None
    username = None
    password = None
    svn_url = None

    for o, a in opts:
        #print o
        #print a
        if o in ("-r", "--production-round"):
            pround = int(a)
        elif o in ("-o", "--order"):
            order = a
        elif o in ("-f", "--file-url"):
            file_url = a
        elif o in ("-u", "--username"):
            username = a
        elif o in ("-p", "--password"):
            password = a
        elif o in ("-s", "--svn-url"):
            svn_url = a
        elif o in ("-h", "--help"):
            usage()
            return
        else:
            usage()
            return

    if pround == None:
        usage()
        print "No production round given"
        return
    if order == None:
        usage()
        print "No order given"
        return
    if file_url is None:
        usage()
        print "No file URL given"
        return
    if username is None:
        usage()
        print "No username given"
        return
    if password is None:
        usage()
        print "No password given"
        return
    if svn_url is None:
        usage()
        print "No SVN URL given"
        return

    commitOrder(pround, order, file_url, username, password, svn_url)


def commitOrder(pround, order, file_url, username, password, svn_url):

    tmp_dir = tempfile.mkdtemp()

    fs_pround_dir = tmp_dir + "/" + "%05d" % pround
    filename = order + "_" + file_url.split("/")[-1]
    file_fullpath = fs_pround_dir + "/" + filename

    subprocess.call(["svn", 
        "checkout", 
        "--username", username,
        "--password", password,
        "--non-recursive", 
        svn_url, tmp_dir])

    subprocess.call(["svn", 
        "update",
        fs_pround_dir,
        "--non-recursive"])

    try:
        os.makedirs(fs_pround_dir)
    except OSError as exc: # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(fs_pround_dir):
            pass
        else: raise

    subprocess.call(["svn", 
        "add",
        fs_pround_dir,
        "--non-recursive"])

    subprocess.call(["svn", 
        "commit",
        "-m", "fab order folder update",
        tmp_dir])

    urllib.urlretrieve(file_url, file_fullpath)

    subprocess.call(["svn", 
        "add",
        file_fullpath])

    subprocess.call(["svn", 
        "commit",
        "-m", "fab order file update",
        tmp_dir])
    

if __name__ == "__main__":
    main()
