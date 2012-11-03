# usage:
#    makepanelizerxml2.py -f destination-folder -u username -pwd password -w worksheet
#    creates a panelizer.xml file that can be used for further steps

import getopt, sys
from datetime import date
import gdata.docs
import gdata.docs.service
import gdata.spreadsheet.service
import re, os, os.path
import ConfigParser
import sys
import traceback
from pprint import pprint

def usage():
    print """
usage:
    makepanelizerxml2.py -f folder-destination -u username -p password -w worksheet
    creates a panelizer.xml file that can be used for further steps
"""
    
           
def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hf:u:p:w:", ["help", "folder", "username", "password","worksheet"])
    except getopt.GetoptError, err:
        # print help information and exit:
        print str(err) # will print something like "option -a not recognized"
        usage()
        sys.exit(2)
        
    destFolder = None
    username = None
    password = None
    worksheet = None
    
    for o, a in opts:
        #print o
        #print a
        if o in ("-f", "--folder"):
            destFolder = a
        elif o in ("-u", "--username", "user"):
            username = a
        elif o in ("-p", "--password", "pwd"):
            password = a
        elif o in ("-w", "--worksheet", "sheet"):
            worksheet = a
        elif o in ("-h", "--help"):
            usage()
            sys.exit(2)
        else:
            assert False, "unhandled option"
    
    if not(destFolder) or not(username) or not(password) or not(worksheet):
        usage()
        sys.exit(2)
        
    try:
        # Connect to Google
        gd_client = gdata.spreadsheet.service.SpreadsheetsService()
        gd_client.email = username
        gd_client.password = password
        gd_client.ProgrammaticLogin()
    except:
        traceback.print_exc()    
        sys.exit(1)

    print "connected"

    # Query for the rows
    print "loading spreadsheet..."
    q = gdata.spreadsheet.service.DocumentQuery()
    q['title'] = "Fritzing Fab Orders"
    q['title-exact'] = 'true'
    feed = gd_client.GetSpreadsheetsFeed(query=q)
    if feed == None:
        print "no spreadsheet feed"
        sys.exit(1)
    
    print "got spreadsheet feed"
    spreadsheet_id = feed.entry[0].id.text.rsplit('/',1)[1]    
    wsfeed = gd_client.GetWorksheetsFeed(spreadsheet_id)
    if wsfeed == None:
        print "no worksheet feed"
        sys.exit(1)
    
    print "checking entries"
    #pprint(vars(wsfeed))
    worksheet_id = None
    for entry in wsfeed.entry:
        print "worksheet", entry.title.text
        if entry.title.text and (entry.title.text == worksheet):
            worksheet_id = entry.id.text.rsplit('/',1)[1]
            break
            
    if not(worksheet_id):
        print "worksheet", worksheet, "not found"
        sys.exit(0)

    print "checking rows", spreadsheet_id, worksheet_id
    lines = []
    listFeed = gd_client.GetListFeed(spreadsheet_id, worksheet_id);
    #pprint(vars(listFeed))
    rows = listFeed.entry
    
    boardsInOrder = {}
    boardsInXml = {}
    for row in rows:
        #print row.custom
        filename = row.custom['filename'].text
        orderNumber = row.custom['ordernr'].text
        if filename and orderNumber:
            if orderNumber in boardsInOrder:
                boardsInOrder[orderNumber]  += 1
            else:
                boardsInOrder[orderNumber]  = 1
                boardsInXml[orderNumber] = 1
            

    for row in rows:
        filename = row.custom['filename'].text
        orderNumber = row.custom['ordernr'].text
        if filename and orderNumber:
            boardCount = row.custom['boards'].text
            if not(boardCount):
                boardCount = 1
            optional = row.custom['optional'].text
            if not(optional):
                optional = 0
            required = row.custom['count'].text
            if not(required):
                required = 0
            xml =  "<board name='{0}_{5}of{6}_x{1}c_x{4}b_{2}' requiredCount='{1}' maxOptionalCount='{3}' inscription='{0}' boardCount='{4}' thisFzz='{5}' fzzCount='{6}' inscriptionHeight='2mm' originalName='{2}' />\n"
            if orderNumber == "products":
                xml =  "<board name='{2}' requiredCount='{1}' maxOptionalCount='{3}' inscription='' boardCount='{4}' thisFzz='{5}' fzzCount='{6}' inscriptionHeight='2mm' originalName='{2}' />\n"
            thisBoard = boardsInXml[orderNumber]
            boardsInXml[orderNumber] += 1
            xml = xml.format(orderNumber, required, filename, optional, boardCount, thisBoard, boardsInOrder[orderNumber])
            lines.append(xml)
            #print xml
        
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



