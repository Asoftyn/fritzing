

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
    makeitems.py -f {fzz folder} -g {item group} -d {item description} -b {bom name}
    creates new items from the set of fzz files in the fzz folder and adds them to a bom if there is a -b option
    the description and group are copied to all items--default of -d is 'Fritzing Fab order'
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
    
    #print response.url
    #print

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
        
def responseOK(response, name, value):
    try:
        message = response.json().get("message")
    #print name, value, type(value), "message", type(message[0].get(name))
        return isinstance(message, list) and len(message) > 0 and message[0].get(name).lower() == value.lower()
    except:
        return False        

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
    parser.add_option('-g', '--group', dest="group" )
    parser.add_option('-d', '--description', default="Fritzing Fab order", dest="description")
    parser.add_option('-f', '--folder', dest="folder" )
    parser.add_option('-b', '--bom', dest="bom")
    (options, args) = parser.parse_args()
    
    if not options.folder:
        parser.error("folder argument not given")
        return
        
    if not options.group:
        parser.error("group argument not given")
        return
        
    destFolder = options.folder
    username = None
    password = None
    group = options.group
    description = options.description
    bom = options.bom
    
    #print "description", description
    
    cdirname, cfilename = os.path.split(os.path.abspath(__file__))
    cfile = os.path.join(cdirname, "makepanelizer2.config")
    if not os.path.isfile(cfile):
        usage()
        print "Configuration file %s is missing!" % cfile
        return
        
    c = ConfigParser.ConfigParser()
    c.read(cfile)
        
    password = c.get('general', 'password').strip()
    username = c.get('general', 'username').strip()
    server = c.get('general', 'server').strip()
    
    if username == None:
        username = raw_input('Enter a user: ')
        
    if username == None:
        usage()
        return
    
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
        return
        
    try:
        # Connect
        login(server, username, password)
    except:
        traceback.print_exc()    
        return

    print "connected"
    
    bomdoc = None
    bomrows = None
    if bom:
        bomdoc = get_doc(server, "BOM", bom)
        if responseOK(bomdoc, "name", bom):
            print "bom", bom, "found"
            bomrows = bomdoc.json().get("message")
        else:
            print "bom", bom, "not found, please create it"
            return
    
    for filename in os.listdir(destFolder):
        if not filename.endswith(".fzz"):
            continue
            
        doc = None
        gotItem = False
        try:
            doc = get_doc(server, "Item", filename)
            if responseOK(doc, "name", filename):
                print filename, "is already an item"
                gotItem = True
                    
        except:
            pass
            
        if not gotItem:    
            response = insert(server, [{
                "doctype":"Item",
                "item_name": filename,
                "item_code": filename,
                "item_group": group,
                "description": description
                }])
            gotItem = responseOK(response, "name", filename)
            if gotItem:
                print filename, "item inserted"
            else:
                print filename, "insert failed"

        if not gotItem:
            continue
        
        if not bomdoc:
            continue
            
        already = False
        for row in bomrows:
            if row.get("item_code") == filename and row.get("doctype") == "BOM Item":
                print "already got BOM Item", filename
                already = True
                break
                
        if not already:
            response = insert(server, [{
            "doctype":"BOM Item",
            "item_code": filename,
            "optional_priority":0,
            "qty":1.0, 
            "optional":0, 
            "stock_uom":"Nos",
            "count":1,
            "amount":0.00,
            "parenttype":"BOM",
            "parentfield":"bom_materials",
            "parent":bom
            }])

            if responseOK(response, "item_code", filename):
                print "added bom item", filename
            else:
                print "failed to add BOM item"
                pprint(vars(response))
                    

            

        
    
    
if __name__ == "__main__":
    main()



