# usage:
#    makepanelizerxml2.py -f destination-folder -u username -pwd password -w worksheet
#    creates a panelizer.xml file that can be used for further steps

import getopt, sys
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
	makepanelizerxml2.py -f {order destination folder} -b {bom  name}
	creates a panelizer.xml file that can be used for further steps
	uses makepanelizer2.config file to load username, password, and document name
"""

def shoveParams(srvr, params):
	"""
		for some reason the requests module is doubling the parameters in the request string
		so shoveParams is a hack to get around this problem
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

    if response.json() and ("exc" in response.json()) and response.json()["exc"]:
        print response.json()["exc"]

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

def main():
	try:
		opts, args = getopt.getopt(sys.argv[1:], "hf:b:", ["help", "folder", "bom"])
	except getopt.GetoptError, err:
		# print help information and exit:
		print str(err) # will print something like "option -a not recognized"
		usage()
		sys.exit(2)
		
	destFolder = None
	username = None
	password = None
	bom = None
	server = None
	
	for o, a in opts:
		#print o
		#print a
		if o in ("-f", "--folder"):
			destFolder = a
		elif o in ("-b", "--bom"):
			bom = a
		elif o in ("-h", "--help"):
			usage()
			sys.exit(2)
		else:
			assert False, "unhandled option"
		
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
		
	if not(bom) :
		usage()
		print "bom name missing"
		sys.exit(2)
		
	try:
		# Connect
		login(server, username, password)
	except:
		traceback.print_exc()    
		sys.exit(1)

	print "connected"
	
	doc = None
	try:
		doc = get_doc(server, "BOM", bom)
	except:
		traceback.print_exc()    
		sys.exit(1)
	
	print "checking rows"
	rows = doc.json().get("message")
	theFormat =  "<board name='{0}' requiredCount='{1}' maxOptionalCount='{2}' />\n"
	lines = []
	
	
	print "row count", len(rows)
	print
	for row in rows:
		#print row
		#print
		
		filename = row.get('item_code')
		if filename:
			originalname = filename
			optional = row.get('optional')
			required = row.get('count')

			#print "opt", optional, required
			
			if optional == None:
				continue
				
			if required == None:
				continue
				
			theLine = theFormat.format(filename, required, optional)
			lines.append(theLine)

		
	outfile = open(os.path.join(destFolder, "panelizer.xml"), "w")
	today = date.today()
	outfile.write("<panelizer width='550mm' height='330mm' small-width='205mm' small-height='330mm' spacing='6mm' border='0mm' prefix='{0}'>\n".format(today.strftime("%Y.%m.%d")))
	outfile.write("<paths>\n")
	productDir = "../../products"
	outfile.write("<path>{0}</path>\n".format(productDir))
	outfile.write("</paths>\n")
	outfile.write("<boards>\n")    
	for l in lines:
		outfile.write(l)
	outfile.write("</boards>\n")    
	outfile.write("</panelizer>\n")    
	outfile.close()
	
	
	
	
if __name__ == "__main__":
	main()



