#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys, os, getopt, errno, requests, json, ConfigParser
from pprint import pprint
import datetime

    
def usage():
    print """
usage:
    erp.py -r <production-round> 

    Connects to ERPnext to create
    - a customer
    - a shipping address for the customer
    - a fab sales order for the customer
    - an entry for this order in the next production round (BOM)
    
    Required data:
    - erp: url, user, password
    - id: production-round, order-number
    - per order item: filename, quantity, total price
    - customer: username, fullname, company, street, optional, country, zip, taxzone, express-shipping
"""


def main():
    
    username = None
    password = None
    erp_url = None
    
    try:
        cdirname, cfilename = os.path.split(os.path.abspath(__file__))
        cfile = os.path.join(cdirname, "erp.config")        
        c = ConfigParser.ConfigParser()
        c.read(cfile)
            
        password = c.get('general', 'password').strip()
        username = c.get('general', 'username').strip()
        erp_url = c.get('general', 'server').strip()
    except:
        print "unable to find config file"
        usage()
        return
        
    if username == None or password == None or erp_url == None:
        print "config file missing username, password, or server"
        return


    try:
        opts, args = getopt.getopt(sys.argv[1:], "hr:", ["help", "production-round"])
    except getopt.GetoptError, err:
        # print help information and exit:
        print str(err) # will print something like "option -a not recognized"
        usage()
        sys.exit(2)

    pround = None

    for o, a in opts:
        #print o
        #print a
        if o in ("-r", "--production-round"):
            pround = int(a)
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

    try:
        pround = str(pround).zfill(6)
    except:
        pass

    # dummy test order (dates are in isoformat)
    submitFabOrder(erp_url, username, password, 
        pround, "76543210", [{'filename':"76543210_ÜntitledSketch.fzz", 'quantity':1, 'price':35.43, 'size':30.32}, 
        {'filename':"76543210_Test.fzz", 'quantity':3, 'price':120.12, 'size':135.23}], 199.00,
        u"dummy3", u"Peter Dümmy", u"ACME Inc.", u"123 Dümmmy Str", 
        u"", u"12345 AZ", "Berlin", u"Germany", 
        "me@acme.com", u"+49 (0)30/12345678", "Germany", False,
        "PayPal", "236741787828423", "2013-02-08", "2013-02-17")


def submitFabOrder(erp_url, erp_user, erp_password,
        productionRound, orderNumber, orderItems, grandTotal,
        customerUserName, customerFullName, customerCompany, customerStreet, customerOptional, 
        customerZIP, customerCity, customerCountry, customerEmail, customerPhone, customerTaxZone, shippingExpress,
        paymentType, paymentId, orderDate, deliveryDate):

    # XXX: support Unicode
    # XXX: what should happen if the script fails at some point?

    response = login(usr=erp_user, pwd=erp_password, server=erp_url)
    print "LOGIN", response.json()["message"]

    result = createCustomer(erp_url, customerUserName, customerFullName, customerCountry)
    print "CREATE customer", result
    if result:

        print "CREATE address", createShippingAddress(erp_url, customerUserName, customerFullName, 
            customerCompany, customerStreet, customerOptional, customerZIP, customerCity, customerCountry,
            customerEmail, customerPhone)

        for orderItem in orderItems:
            itemID = createItem(erp_url, productionRound, orderItem.get('filename', None))
            print "CREATE item", itemID
            if itemID:
                orderItem['itemID'] = itemID
                print "ADDTO bom", addToBOM(erp_url, productionRound, itemID, 
                    orderItem.get('quantity', 0), orderItem.get('size', 0))

        print "CREATE sales order", createSalesOrder(erp_url, customerUserName, productionRound, 
            orderItems, grandTotal, customerCountry, customerTaxZone, shippingExpress, orderNumber, orderDate, deliveryDate)

        print "CREATE journal voucher", createJournalVoucher(erp_url, customerUserName, customerFullName, 
            productionRound, orderItems, grandTotal, orderNumber, orderDate, paymentType, paymentId)

    #response = get_doc(erp_url, "Sales Order", "SO00097")
    #print "sales order"
    #pprint(vars(response))


class AuthError(Exception): pass

def login(usr=None, pwd=None, server=None):
    response = requests.get(server, params = {
        "cmd": "login",
        "usr": usr,
        "pwd": pwd
    })

    #pprint(vars(response))

    if response.json().get("message")=="Logged In":
        global sid
        sid = response.cookies["sid"]
        return response
    else:
        raise AuthError
    
    
def createCustomer(server, username, fullname, country):
    response = get_doc(server, "Customer", username)
    if responseOK(response, "name", username):
        return True # customer already exists

    # TODO: if customer is a company, then the customer_name should be the company's name,
    #       and the fullname should be inserted as the contact person

    response = insert(server, [{
        "doctype":"Customer",
        "customer_name": username,
        "customer_type": "Individual",
        "company": "IxDS GmbH",
        "customer_group": "Fab Customers",
        "territory": country
    }])
    
    #pprint(vars(response))
    if not responseOK(response, "name", username):
        print "customer insert failed"
        return False
                
    doc = get_doc(server, "Customer", username)
    if not responseOK(doc, "name", username):
        print "customer get doc failed"
        return False
        
    rows = doc.json().get("message")
    for row in rows:
        if row.get("customer_name") == username:
            row["customer_name"] = fullname
            break
            
    result = update(server, rows)
    print "UPDATE customer"    
    return responseOK(result, "customer_name", fullname)


def createShippingAddress(server, username, fullname, company, street, optional, zip, city, country, email, phone):
    # TODO: what happens if an address gets updated, e.g., just the street

    shippingName = username+"-Shipping"
    response = get_doc(server, "Address", shippingName)
    if responseOK(response, "name", shippingName):
        return True # address already exists

    response = insert(server, [{
            "doctype":"Address",
            "customer": username,
            "address_title": username,
            "customer_company": company,
            "address_line1": street,
            "address_line2": optional,
            "pincode": zip,
            "city": city,
            "country": country,
            "email_id": email,
            "phone": phone,
            "address_type": "Shipping",
            "is_shipping_address": 1,
        }])
    
    #print "address response"
    #pprint(vars(response))
    #print

    if not responseOK(response, "address_title", username):
        print "address insert failed"
        return False

    doc = get_doc(server, "Address", shippingName)
    if not responseOK(doc, "name", shippingName):
        print "get address failed"
        return False
        
    rows = doc.json().get("message")
    for row in rows:
        if row.get("address_title") == username:
            row["address_title"] = fullname
            break
            
    result = update(server, rows)
    print "UPDATE"    
    return responseOK(result, "address_title", fullname)


def createItem(server, productionRound, file_url):
    response = get_doc(server, "Item Group", productionRound)
    if not responseOK(response, "item_group_name", productionRound):
        response = insert(server, [{
            "doctype":"Item Group",
            "parent_item_group": "Fab orders",
            "is_group": "No",
            "item_group_name": productionRound
            }])
        if not responseOK(response, "item_group_name", productionRound):
            print "item group failed"
            pprint(vars(response))
            return None
        
    basename = os.path.basename(file_url)
    response = get_doc(server, "Item", basename)
    #pprint(vars(response))
    if responseOK(response, "item_name", basename):
        return basename     #  already exists
    
    response = insert(server, [{
        "doctype":"Item",
        "item_name": basename,
        "item_code": basename,
        "description": "Fritzing Fab order",
        "item_group": productionRound
        }])

    result = responseOK(response, "item_name", basename)
    if not result:
        print "insert item failed"
        pprint(vars(response))
        return None

    return basename


def createSalesOrder(server, username, productionRound, orderItems, grandTotal, 
        country, taxZone, shippingExpress, orderNumber, orderDate, deliveryDate):
    # TODO: support EU with VAT ID
    # TODO: still need to press "recalculate values" and "get taxes and charges" in erpnext ui

    response = get_doc(server, "Sales Order", filters={"po_no":orderNumber})
    if responseOK(response, "po_no", orderNumber):
        print "got sales order", orderNumber, response.json().get("message")[0].get("name")
    else:  
        if taxZone.lower() == "germany":
            chargeType = "VAT+Shipping DE"
            if shippingExpress:
                chargeType = "VAT+Express DE"
        elif taxZone.lower() == "eu":
            chargeType = "VAT+Shipping EU"
            if shippingExpress:
                chargeType = "VAT+Express EU"
        else:
            chargeType = "Shipping World"
            if shippingExpress:
                chargeType = "Express World"
   
        # this is the total net price of all orders incl. checking, without taxes/shipping
        total = 0
        for orderItem in orderItems:
            total += orderItem.get('price', 0)

        response = insert(server, [{
            "doctype":"Sales Order",
            "po_no":orderNumber,
            "customer": username,
            "customer_name":username,
            "company":"IxDS GmbH",
            "territory":country,
            "charge":chargeType,
            "customer_group":"Fab Customers",
            "order_type": "Sales",
            "naming_series":"SO",
            "fiscal_year":orderDate[0:4],
            "transaction_date":orderDate,
            "currency":"EUR",
            "price_list_currency":"EUR",
            "conversion_rate":1.0,
            "plc_conversion_rate":1.0,
            "price_list_name":"Standard",
            "grand_total_export":total,
            "delivery_date":deliveryDate
            }])
        if not responseOK(response, "po_no", orderNumber):
            print "unable to create sales order", orderNumber
            return False

    rows = response.json().get("message")
    salesOrderName = rows[0].get("name")
    if not salesOrderName.startswith("SO"):
        print "weird sales order name", salesOrderName
        return False

    print "sales order name", salesOrderName

    for orderItem in orderItems:
        already = False
        singlePrice = orderItem.get('price') / orderItem.get('quantity')
        for row in rows:
            itemcode = ensure_unicode(row.get('item_code'))
            itemid = ensure_unicode(orderItem.get('itemID'))
            if itemcode == itemid and abs(row.get('ref_rate') - singlePrice) < 0.1:
                already = True
                break

        if not already:
            response = insert(server, [{
                "ref_rate":singlePrice,
                "stock_uom":"Nos",
                "reserved_warehouse":"Loch Current Production",
                "description":"Fritzing Fab order",
                "parent":salesOrderName,
                "brand":"Fritzing",
                "item_code":orderItem.get('itemID'),
                "item_group": productionRound,
                "qty":orderItem.get('quantity'),
                "transaction_date":orderDate,
                "doctype":"Sales Order Item",
                "item_name":orderItem.get('itemID'),
                "parenttype":"Sales Order",
                "parentfield":"sales_order_details"
            }])
            if not responseOK(response, "item_code", orderItem.get('itemID')):
                print "failed to created sales order item", orderItem.get('itemID')
                return False

            print "sales order item", orderItem.get('itemID'), "already =", already

    return True


def createJournalVoucher(server, username, fullname, productionRound, orderItems, grandTotal,
        orderNumber, orderDate, paymentType, paymentId):
    # TODO: Should paymentType also be handled here?

    account = username + " - IXDS"
    response = get_doc(server, "Journal Voucher", filters={"bill_no":orderNumber})
    if responseOK(response, "bill_no", orderNumber):
        print "got journal voucher", orderNumber
    else:
        response = insert(server, [{
        "doctype":"Journal Voucher",
        "bill_no":orderNumber,
        "naming_series":"JV",
        "cheque_date":orderDate,
        "voucher_date":orderDate,
        "posting_date":orderDate,
        "voucher_type":"Bank Voucher",
        "cheque_no":paymentId,
        "company":"IxDS GmbH",
        "fiscal_year":orderDate[0:4],
        "total_credit":grandTotal,
        "total_amount":grandTotal,
        "pay_to_recd_from":account,
        "total_debit":grandTotal
        }])
        if not responseOK(response, "bill_no", orderNumber):
            print "unable to create journal voucher", orderNumber
            pprint(vars(response))
            print
            return False

    
    rows = response.json().get("message")
    jvName = rows[0].get("name")
    if not jvName.startswith("JV"):
        print "weird journal voucher name", jvName
        return False
    
    status = rows[0].get("docstatus")
    if status == 1:
        # already submitted, we are done
        print "journal voucher", orderNumber, "already submitted"
        return True
    
    already = False
    for row in rows:
        if row.get("doctype") == "Journal Voucher Detail":
            # really unlikely that the Journal Voucher that exists is incomplete, so bail if any detail entries are found
            already = True
            break

    
    if not already:
        response = insert(server, [{
        "account":account,
        "doctype":"Journal Voucher Detail",
        "parent":jvName,
        "credit":grandTotal,
        "parenttype":"Journal Voucher",
        "against_account":"Paypal order@ixds.de - IXDS",
        "parentfield":"entries"
        }])
        if not responseOK(response, "account", account):
            print "failed to created journal voucher detail credit" 
            pprint(vars(response))
            print
            return False

        response = insert(server, [{
        "account":"Paypal order@ixds.de - IXDS",
        "doctype":"Journal Voucher Detail",
        "parent":jvName,
        "debit":grandTotal,
        "balance":"0.00",
        "parenttype":"Journal Voucher",
        "against_account":account,
        "parentfield":"entries"
        }])
        if not responseOK(response, "against_account", account):
            print "failed to created journal voucher detail debit"
            pprint(vars(response))
            print
            return False
            
    doc = get_doc(server, "Journal Voucher", filters={"bill_no":orderNumber})
    if responseOK(doc, "bill_no", orderNumber):
        rows = doc.json().get("message")
        response = submit(server, rows)
        if not responseOK(response, "bill_no", orderNumber):
            print "submit failed"
            pprint(vars(response))
            return False

    return True


def addToBOM(server, productionRound, entryID, quantity, size):
    # TODO: also include board size, number of boards per sketch, and more?

    bomItemName = "FabRun" + productionRound
    bomName = "BOM/" + bomItemName + "/001"     # always 001?

    # first create an item for the BOM itself, as opposed to the items in the BOM (entryID is one of those items)
    response = get_doc(server, "Item", bomItemName)
    if not responseOK(response, "item_code", bomItemName):  
        if not makeItemForBOM(server, productionRound, bomItemName):
            return False 

    doc = get_doc(server, "BOM", bomName)
    if not responseOK(doc, "name", bomName):
        response = insert(server, [{
            "doctype":"BOM",
            "item": bomItemName
            }])
        print "creating new BOM"
        #pprint(vars(response))

        doc = get_doc(server, "BOM", bomName)
        if not responseOK(doc, "name", bomName):
            print "unable to create BOM", productionRound
            return False

    already = False
    rows = doc.json().get("message")
    for row in rows:
        if ensure_unicode(row.get("item_code")) == ensure_unicode(entryID) and row.get("doctype") == "BOM Item" and row.get("count") == quantity:
            print "already got BOM Item", entryID
            already = True
            break

    if not already:
        response = insert(server, [{
        "doctype":"BOM Item",
        "item_code": entryID,
        "optional_priority":0,
        "qty":1.0, 
        "optional":0, 
        "stock_uom":"Nos",
        "count":quantity,
        "pcb_size":size/quantity,
        "amount":0.00,
        "parenttype":"BOM",
        "parentfield":"bom_materials",
        "parent":bomName
        }])
        print "adding bom item", entryID
        if not responseOK(response, "item_code", entryID):
            print "failed to add BOM item"
            #pprint(vars(response))
            return False

    return True


def makeItemForBOM(server, productionRound, bomItemName):
    response = get_doc(server, "Item", bomItemName)
    if responseOK(response, "name", bomItemName):
        return True

    response = insert(server, [{
        "doctype":"Item",
        "item_name": bomItemName,
        "item_code": bomItemName,
        "is_manufactured_item":"Yes",
        "is_pro_applicable":"Yes",
        "is_sub_contracted_item":"Yes",
        "stock_uom":"Nos",
        "description": "Item for the BOM, Fab Production Run " + productionRound,
        "item_group": productionRound
        }])
    if not responseOK(response, "item_name", bomItemName):
        print "unable to create Item for BOM " + productionRound
        return False

    doc = get_doc(server, "Item", bomItemName)
    if not responseOK(doc, "name", bomItemName):
        print "get Item for BOM failed"
        return False
      
    newname = "Fab production run " + productionRound  
    rows = doc.json().get("message")
    #print "bom's item"
    #print rows
    #print
    for row in rows:
        if row.get("item_name") == bomItemName:
            row["item_name"] = newname
            print "setting new name", newname
            break
            
    result = update(server, rows)
    print "UPDATE"    
    if not responseOK(result, "item_name", newname):
        print "Item for BOM update failed", productionRound
        #pprint(vars(result))
        return False

    return True


def insert(server, doclist):
    return request(server, {
        "cmd": "webnotes.client.insert",
        "doclist": json.dumps(doclist)
    })


def update(server, doclist):
    return request(server, {
        "cmd": "webnotes.client.save"
        },
        {
        "doclist": json.dumps(doclist) 
        }
        )

def submit(server, doclist):
    return request(server, {
        "cmd": "webnotes.client.submit"
        },
        {
        "doclist": json.dumps(doclist) 
        }
        )
        
def delete(server, doctype, name):
    return request(server, {
        "cmd": "webnotes.model.delete_doc",
        "doctype": doctype,
        "name": name
    })


def get_doc(server, doctype, name = None, filters = None):
    params = {
        "cmd": "webnotes.client.get",
        "doctype": doctype,
    }
    if name:
        params["name"] = name
    if filters:
        params["filters"] = json.dumps(filters)
    ret = request(server, params)
    return ret


def request(server, params = None, data = None):
    if not sid: login()
    response = requests.post(server, params = params, data = data, cookies = {"sid": sid})
    return response


def responseOK(response, name, value):
    try:
        message = response.json().get("message")
        if not isinstance(message, list) or len(message) == 0:
            return False
                    
        messagevalue = ensure_unicode(message[0].get(name))
        value = ensure_unicode(value)
        return messagevalue.lower() == value.lower()
    except:
        return False

def ensure_unicode(value):
    if isinstance(value, str):
        return unicode(value, "UTF-8")

    return value


if __name__ == "__main__":
    main()

