

import optparse, sys, os, os.path, re
import json, urllib, urllib2, ConfigParser, traceback, requests
import xml.dom.minidom
from pprint import pprint
    
def usage():
    print """
usage:
    updateqty.py -f [panelizer xml path] -b {bom name}
    puts the produced quantity back into the BOM  
    """		

def login(server, usr, pwd):

    params = {
        "cmd": "login",
        "usr": usr,
        "pwd": pwd
    }

    response = requests.get(server, params=params)
    
    #print "url", response.url
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
        

def update(server, doclist):
    return request(server, {
        "cmd": "webnotes.client.save"
        },
        {
        "doclist": json.dumps(doclist) 
        }
        )
    

def request(server, params=None, data=None):
    global sid
    
    if not sid: login()
    
    if data == None:
        response = requests.post(server, cookies = {"sid": sid}, params=params)
        #print "params", params
        #print "data", data
        print response.url
        print "request"
        pprint(vars(response.request))
        print

        #if debug and response.json() and ("exc" in response.json()) and response.json()["exc"]:
        #    print response.json()["exc"]

        return response
        
    else:
        print "trying urllib2"
        print "data",data
        form = {"form": data}
        req = urllib2.Request(server + "?" + urllib.urlencode(params), urllib.urlencode(form))
        req.add_header('Cookie', 'sid=' + sid)
        
        pprint(vars(req))
        f = urllib2.urlopen(req)
        result = f.read()
        f.close()
        
        print "result", result
    

def main():
    
    parser = optparse.OptionParser()
    parser.add_option('-f', '--file',dest="file" )
    parser.add_option('-b', '--bom',dest="bom" )
    (options, args) = parser.parse_args()

    fromName = options.file
    if fromName == None:
        print "no -f argument to panelizer.xml"
        usage()
        return
        
    bom = options.bom
    if bom == None:
        print "no -b BOM argument"
        usage()
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
        
    try:
        # Connect
        login(server, username, password)
    except:
        traceback.print_exc()    
        sys.exit(1)

    print "connected"

    doc = get_doc(server, "BOM", bom)
    #pprint(vars(doc))
    rows = doc.json().get("message")
    if type(rows) == list and len(rows) > 0 and rows[0].get("name") == bom:
        print bom, "is found"  
    
    boardNodes = boards.getElementsByTagName("board")
    for board in boardNodes:
        name = board.getAttribute("name")
        if name == None:
            continue
        
        if len(name) == 0:
            continue
        
        produced = 0
        try:
            produced = int(board.getAttribute("produced"))
        except:
            print "'produced' not found for", name
            continue
            
        for row in rows:
            if row.get("item_code") == name and row.get("qty") != None:
                row["qty"] = produced
                print "updating name", name, "to", produced
    
    update(server, rows)

            
            
if __name__ == "__main__":
    main()



