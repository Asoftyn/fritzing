

import optparse, sys
from datetime import date
import re, os, os.path
import ConfigParser
import sys
import traceback
from pprint import pprint
import requests
import json
import urllib

class AuthError(Exception): 
    pass

def usage():
    print """
usage:
    makeitems.py -f {fzz folder} -g {item group} -d {item description}
    creates new bom items from the set of fzz files in the fzz folder.
    the description and group are copied to all items--default values are '042' and 'Fritzing Fab order' respectively
    if the fzz already exists as an item, does nothing.
"""

def shoveParams(srvr, params):
    """
        for some reason the requests module is doubling the parameters in the request string
        so shoveParams is a hack to get around this problem
        this doesn't seem to happen under linux
    """
    
    if len(params) == 0:
        return srvr

    sep = "?"
    url = srvr
    for k in params.keys():
        url += (sep + urllib.quote(k) + "=" + urllib.quote(params[k]))
        sep = '&'
            
    return url	
    
def request(server, params):
    if not sid: login()
    url = shoveParams(server, params)
    response = requests.post(url, cookies = {"sid": sid})
    
    print response.url
    print

    #if response.json() and ("exc" in response.json()) and response.json()["exc"]:
        #print response.json()["exc"]

    return response

def login(server, usr, pwd):

    params = {
        "cmd": "login",
        "usr": usr,
        "pwd": pwd
    }

    url = shoveParams(server, params)
    
    #print "login to", url
    
    response = requests.get(url)
    
    #print response.url
    #print
    #print response.json()
    #print
    
    if response.json().get("message")=="Logged In":
        global sid
        sid = response.cookies["sid"]
        return response
    else:
        print "login failed"
        raise AuthError

def get_doc(server, doctype, name):
    ret = request(server, {
        "cmd": "webnotes.client.get",
        "doctype": doctype,
        "name": name
    })
    return ret
    


def insert(server, doclist):
    return request(server, {
        "cmd": "webnotes.client.insert",
        "doclist": json.dumps(doclist)
    })

def main():
    parser = optparse.OptionParser()
    parser.add_option('-g', '--group', default="042", dest="group" )
    parser.add_option('-d', '--description', default="Fritzing Fab order", dest="description")
    parser.add_option('-f', '--folder',dest="folder" )
    (options, args) = parser.parse_args()
    
    if not options.folder:
        parser.error("folder argument not given")
        sys.exit(1)
        
    destFolder = options.folder
    username = None
    password = None
    group = options.group
    description = options.description
    
    print "description", description
    
    cdirname, cfilename = os.path.split(os.path.abspath(__file__))
    cfile = os.path.join(cdirname, "makepanelizer2.config")
    if not os.path.isfile(cfile):
        usage()
        print "Configuration file %s is missing!" % cfile
        sys.exit(2)
        
    c = ConfigParser.ConfigParser()
    c.read(cfile)
        
    password = c.get('general', 'password').strip()
    username = c.get('general', 'username').strip()
    server = c.get('general', 'server').strip()
    
    if username == None:
        username = raw_input('Enter a user: ')
        
    if username == None:
        usage()
        sys.exit(2)
    
    if password == None:
        password = raw_input('Enter a password: ')
        
    if password == None:
        usage()
        return None
        
    if server == None:
        server = raw_input('Enter a server: ')
        
    if server == None:
        usage()
        return None
        
    if not(destFolder):
        usage()
        print "destination folder missing"
        sys.exit(2)
        
    try:
        # Connect
        login(server, username, password)
    except:
        traceback.print_exc()    
        sys.exit(1)

    print "connected"
    
    for filename in os.listdir(destFolder):
        if not filename.endswith(".fzz"):
            continue
            
        doc = None
        try:
            doc = get_doc(server, "Item", filename)
            rows = doc.json().get("message")
            if type(rows) == list and len(rows) > 0 and rows[0].get("name") == filename:
                print filename, "is already an item"
                continue
                    
        except:
            pass
            
        response = insert(server, [{
            "doctype":"Item",
            "item_name": filename,
            "item_code": filename,
            "item_group": group,
            "description": description
            }])
            
        message = response.json().get("message")
        if isinstance(message, list) and len(message) > 0 and message[0].get("item_name")==filename:
            print filename, "item inserted"
            continue
         
        
        print filename, "insert failed"
        print message
        print
            
        
        
    
    
if __name__ == "__main__":
    main()



