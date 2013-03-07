# -*- coding: utf-8 -*-
import urllib
import re
from DateTime import DateTime

from zope.component.hooks import getSite

from Products.CMFCore.utils import getToolByName
from Products.CMFCore import permissions

from email.MIMEText import MIMEText
from email.Utils import formataddr
from smtplib import SMTPRecipientsRefused

from fritzing.fab.interfaces import IFabOrder
from fritzing.fab import _
from fritzing.fab import erp
#from fritzing.fab import svn


def canDelete(self, item):
    portal_membership = getToolByName(item, 'portal_membership')
    if portal_membership.checkPermission(permissions.DeleteObjects, item) \
        and portal_membership.checkPermission(permissions.DeleteObjects, item.aq_parent):
        return True
    return False

def isStateId(self, item, state_id):
    return getStateId(self, item) == state_id


def getStateId(self, item, portal_workflow = None):
    """
    invoke like this:
    getStateId(None, item) or getStateId(None, item, portal_workflow)
    first argument isn't used, pass whatever you like
    """
    if portal_workflow == None:
        portal_workflow = getToolByName(item, 'portal_workflow')
    state_id = portal_workflow.getInfoFor(item, 'review_state')
    return state_id


def getStateTitle(self, item, portal_workflow = None):
    """
    invoke like this:
    getStateTitle(None, item) or getStateTitle(None, item, portal_workflow)
    first argument isn't used, pass whatever you like
    """
    if portal_workflow == None:
        portal_workflow = getToolByName(item, 'portal_workflow')
    state_id = getStateId(self, item, portal_workflow)
    state_title = portal_workflow.getTitleForStateOnType(state_id, item.portal_type)
    return state_title


def encodeFilename(self, filename):
    if filename is None:
        return None
    else:
        if isinstance(filename, unicode):
            filename = filename.encode('utf-8')
        return urllib.quote_plus(filename)


def calculateDiscount(totalArea):
    # choose discount
    if (totalArea < 50):
        return 0.70
    elif (totalArea < 100):
        return 0.60
    elif (totalArea < 200):
        return 0.50
    elif (totalArea < 500):
        return 0.40
    else:
        return 0.35


def calculateSinglePrice(area, count):
    totalArea = area * count
    return totalArea * calculateDiscount(totalArea)


def recalculatePrices(faborder):
    faborders = faborder.aq_parent

    # sum up the areas of all sketches and the number of quality checks
    faborder.area = 0
    faborder.numberOfQualityChecks = 0
    for sketch in faborder.listFolderContents():
        faborder.area += sketch.copies * sketch.area
        if sketch.check:
            faborder.numberOfQualityChecks += 1
    
    faborder.pricePerSquareCm = calculateDiscount(faborder.area)

    # calculate totals
    faborder.priceNetto = faborder.area * faborder.pricePerSquareCm
    faborder.priceQualityChecksNetto = faborder.numberOfQualityChecks * faborders.checkingFee
    faborder.priceTotalNetto = faborder.priceNetto + faborder.priceQualityChecksNetto
    
    # shipping and taxes
    faborder.priceShipping = faborders.shippingWorld
    if faborder.shippingExpress:
        faborder.priceShipping = faborders.shippingWorldExpress
    faborder.taxesPercent = faborders.taxesWorld
    if faborder.shipTo == u'germany':
        faborder.priceShipping = faborders.shippingGermany
        if faborder.shippingExpress:
            faborder.shippingExpress = False
            #faborder.priceShipping = faborders.shippingGermanyExpress
        faborder.taxesPercent = faborders.taxesGermany
    elif faborder.shipTo == u'eu':
        faborder.priceShipping = faborders.shippingEU
        if faborder.shippingExpress:
            faborder.priceShipping = faborders.shippingEuExpress
        faborder.taxesPercent = faborders.taxesEU
    
    faborder.taxes = faborder.priceTotalNetto * faborder.taxesPercent / 100.0
    faborder.priceTotalBrutto = faborder.priceTotalNetto + faborder.taxes + faborder.priceShipping


def getCurrentOrders(self, faborders, productionRound=None):
    """Get all current orders to be produced
       options: productionRound - specify an old production round
    """
    if not productionRound:
        productionRound = faborders.currentProductionRound

    catalog = getToolByName(faborders, 'portal_catalog')

    #start = faborders.currentProductionOpeningDate
    #if not start:
    #    start = DateTime.DateTime() - 7
    #end = DateTime.DateTime()
    results = catalog.searchResults({
        'portal_type':'faborder', 
        #'created' : {'query':(start, end), 'range': 'min:max'},
        'productionRound':int(productionRound),
        'sort_on':'Date',
        'sort_order':'reverse',
        'review_state':{
            "query":['in_process', 'in_production', 'completed'],
            "operator" : "or"}
        })
    
    return results


def sendStatusMail(context, justReturn=False):
    """Sends notification on the order status to the orderer and faborders.salesEmail
    """
    mail_text = u""
    charset = 'utf-8'
    
    portal = getSite()
    mail_template = portal.mail_order_status_change
    faborder = context
    faborders = context.aq_parent
    
    from_address = faborders.salesEmail
    from_name = "Fritzing Fab"
    to_address = context.email
    user  = context.getOwner()
    to_name = utf_8_encode(user.getProperty('fullname', ''))
    # we expect a name to contain non-whitespace characters:
    if not re.search('\S', to_name):
        to_name = u"%s" % user
    
    state_id = getStateId(False, context)
    state_title = getStateTitle(False, context)
    closing_date = faborders.nextProductionClosingDate
    delivery_date = faborders.nextProductionDelivery
    checking_fee = faborders.checkingFee
    
    mail_subject = _(u"Your Fritzing Fab order #%s is now %s") % (context.id, state_title.lower())
    
    mail_text = mail_template(
        to_name = to_name,
        to_address = to_address,
        state_id = state_id,
        state_title = state_title,
        closing_date = closing_date,
        delivery_date = delivery_date,
        checking_fee = checking_fee,
        faborder = faborder,
        faborder_id = faborder.id,
        faborder_url = faborder.absolute_url(),
        faborder_shipping_date = faborder.shippingDate,
        faborder_items = faborder.listFolderContents(),
        faborder_price_netto = faborder.priceTotalNetto,
        faborder_price_brutto = faborder.priceTotalBrutto,
        faborder_price_shipping = faborder.priceShipping,
        faborder_taxes = faborder.taxes,
        faborder_taxes_percent = faborder.taxesPercent,
        ship_to = IFabOrder['shipTo'].vocabulary.getTerm(context.shipTo).title,
        )
    
    if justReturn:
        return mail_text
    
    try:
        host = getToolByName(context, 'MailHost')
        # send our copy:
        host.send(
            MIMEText(mail_text, 'plain', charset), 
            mto = formataddr((from_name, from_address)),
            mfrom = formataddr((from_name, from_address)),
            subject = mail_subject,
            charset = charset,
            msg_type="text/plain",
        )
        # send notification for the orderer:
        host.send(
            MIMEText(mail_text, 'plain', charset), 
            mto = formataddr((to_name, to_address)),
            mfrom = formataddr((from_name, from_address)),
            subject = mail_subject,
            charset = charset,
            msg_type="text/plain",
        )
    except SMTPRecipientsRefused:
        # Don't disclose email address on failure
        raise SMTPRecipientsRefused('Recipient address rejected by server')


def sendSketchUpdateMail(sketch, justReturn=False):
    """Sends notification when a sketch file has been replaced
    """
    mail_text = u""
    charset = 'utf-8'
    
    portal = getSite()
    mail_template = portal.mail_sketch_update
    faborder = sketch.__parent__
    faborders = faborder.__parent__
    
    from_address = faborders.salesEmail
    from_name = "Fritzing Fab"
    to_address = faborder.email
    closing_date = faborders.nextProductionClosingDate
    user  = faborder.getOwner()
    to_name = utf_8_encode(user.getProperty('fullname', ''))
    # we expect a name to contain non-whitespace characters:
    if not re.search('\S', to_name):
        to_name = u"%s" % user
        
    mail_subject = _(u"Your Fritzing Fab order #%s has been updated") % (faborder.id,)
    
    mail_text = mail_template(
        to_name = to_name,
        to_address = to_address,
        closing_date = closing_date,
        faborder = faborder,
        sketch = sketch,
        )
    
    if justReturn:
        return mail_text
    
    try:
        host = getToolByName(sketch, 'MailHost')
        # send our copy:
        host.send(
            MIMEText(mail_text, 'plain', charset), 
            mto = formataddr((from_name, from_address)),
            mfrom = formataddr((from_name, from_address)),
            subject = mail_subject,
            charset = charset,
            msg_type="text/plain",
        )
        # send notification for the orderer:
        host.send(
            MIMEText(mail_text, 'plain', charset), 
            mto = formataddr((to_name, to_address)),
            mfrom = formataddr((from_name, from_address)),
            subject = mail_subject,
            charset = charset,
            msg_type="text/plain",
        )
    except SMTPRecipientsRefused:
        # Don't disclose email address on failure
        raise SMTPRecipientsRefused('Recipient address rejected by server')


def commitToSVN(faborder):
    """ Commits the fzzs of a given fab order to commitToSVN
    """
    for sketch in faborder.listFolderContents():
            svn.commitOrder(faborder.productionRound, faborder.id,
                sketch.absolute_url()+"/@@download/orderItem/"+encodeFilename(None,sketch.orderItem.filename),
                faborders.svnLogin, faborders.svnPassword, faborders.svnUrl)


def submitToERP(faborder):
    """ Sends faborder to ERPnext
    """
    # XXX: try/catch and write warning mail to fab@fritzing.org
    # XXX: also call this in SketchUpdateForm, and on cancel

    faborders = faborder.__parent__

    items = []
    for sketch in faborder.listFolderContents():
        item = {}
        item['filename'] = faborder.id + '_' + encodeFilename(None,sketch.orderItem.filename)
        item['quantity'] = int(sketch.copies)
        item['price'] = float('%.2f' % (sketch.copies * sketch.area * faborder.pricePerSquareCm + faborders.checkingFee))
        item['size'] = float(sketch.area)
        items.append(item)

    erp.submitFabOrder(
        faborders.erpUrl, 
        faborders.erpLogin, 
        faborders.erpPassword,
        str(faborder.productionRound).zfill(6), 
        faborder.id, 
        items, 
        faborder.priceTotalBrutto,
        faborder.getOwner().getId(),
        utf_8_encode(faborder.shippingFirstName) + " " + utf_8_encode(faborder.shippingLastName),
        utf_8_encode(faborder.shippingCompany), 
        utf_8_encode(faborder.shippingStreet), 
        utf_8_encode(faborder.shippingAdditional),
        utf_8_encode(faborder.shippingZIP), 
        utf_8_encode(faborder.shippingCity), 
        IFabOrder['shippingCountry'].vocabulary.getTerm(faborder.shippingCountry).title,
        faborder.email, 
        faborder.telephone, 
        faborder.shipTo, 
        faborder.shippingExpress,
        faborder.paymentType, 
        faborder.paymentId, 
        DateTime(faborder.Date()).strftime('%Y-%m-%d'),
        faborders.nextProductionDelivery.strftime('%Y-%m-%d'))


def utf_8_encode(string):
    if isinstance(string, unicode):
        return string.encode('utf-8')
    else:
        return string
