import sys, os, getopt, tempfile, subprocess, urllib, errno#, pysvn

    
def usage():
    print """
usage:
    svn.py -r <production-round> -o <order> -f <file-url> -u <username> -p <password> -s <svn-url>
    
    Commits a given Fritzing file to an SVN repository, into the folder for the given
    production round.
"""

def main():
    global username, password

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


def getSvnLogin( realm, username, may_save ):
    return True, username, password, True


def commitOrder(pround, order, file_url, username, password, svn_url):

    tmp_dir = tempfile.mkdtemp()

    pround_dir = tmp_dir + "/" + "%05d" % pround
    filename = order + "_" + file_url.split("/")[-1]
    file_fullpath = pround_dir + "/" + filename

    client = pysvn.Client()
    client.callback_get_login = getSvnLogin
    client.checkout(svn_url, tmp_dir, recurse=False)
    client.update(pround_dir, recurse=False)

    try:
        os.makedirs(pround_dir)
    except OSError as exc: # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(pround_dir):
            pass
        else: raise

    client.checkin([tmp_dir], "new/update fab round")
    urllib.urlretrieve(file_url, file_fullpath)
    client.add(file_fullpath)
    client.checkin([tmp_dir], "new/updated fab order")


if __name__ == "__main__":
    main()
