import urllib
import re
import DateTime

from zope.component.hooks import getSite

from Products.CMFCore.utils import getToolByName
from Products.CMFCore import permissions

from email.MIMEText import MIMEText
from email.Utils import formataddr
from smtplib import SMTPRecipientsRefused

from fritzing.fab.interfaces import IFabOrder
from fritzing.fab import _


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


def recalculatePrices(faborder):
    faborder.priceNetto = faborder.area * faborder.pricePerSquareCm
    faborder.priceQualityChecksNetto = faborder.numberOfQualityChecks * 4.0
    faborder.priceTotalNetto = faborder.priceNetto + faborder.priceQualityChecksNetto
    
    # shipping and taxes
    faborders = faborder.aq_parent
    faborder.priceShipping = faborders.shippingWorld
    if faborder.shippingExpress:
        faborder.priceShipping = faborders.shippingWorldExpress
    faborder.taxesPercent = faborders.taxesWorld
    if faborder.shipTo == u'germany':
        faborder.priceShipping = faborders.shippingGermany
        if faborder.shippingExpress:
            faborder.priceShipping = faborders.shippingGermanyExpress
        faborder.taxesPercent = faborders.taxesGermany
    elif faborder.shipTo == u'eu':
        faborder.priceShipping = faborders.shippingEU
        if faborder.shippingExpress:
            faborder.priceShipping = faborders.shippingEuExpress
        faborder.taxesPercent = faborders.taxesEU
    
    faborder.taxes = faborder.priceTotalNetto * faborder.taxesPercent / 100.0
    faborder.priceTotalBrutto = faborder.priceTotalNetto + faborder.taxes + faborder.priceShipping


def getCurrentOrders(self, faborders):
    """Get all current orders to be produced with the next batch
    """
    catalog = getToolByName(faborders, 'portal_catalog')
    start = faborders.currentProductionOpeningDate
    if not start:
        start = DateTime.DateTime() - 7
    end = DateTime.DateTime() + 0.1
    results = catalog.searchResults({
        'portal_type': 'faborder', 
        'created' : {'query':(start, end), 'range': 'min:max'},
        'sort_on':'Date',
        'sort_order':'reverse',
        'review_state': 'in_process'})
    return results

def sendStatusMail(context, justReturn=False):
    """Sends notification on the order status to the orderer and faborders.salesEmail
    """
    mail_text = u""
    charset = 'utf-8'
    
    portal = getSite()
    mail_template = portal.mail_order_status_change
    faborders = context.aq_parent
    
    from_address = faborders.salesEmail
    from_name = "Fritzing Fab"
    to_address = context.email
    user  = context.getOwner()
    to_name = user.getProperty('fullname', '')
    # we expect a name to contain non-whitespace characters:
    if not re.search('\S', to_name):
        to_name = u"%s" % user
    
    state_id = getStateId(False, context)
    state_title = getStateTitle(False, context)
    closing_date = faborders.nextProductionClosingDate
    delivery_date = faborders.nextProductionDelivery
    
    mail_subject = _(u"Your Fritzing Fab order #%s is now %s") % (context.id, state_title.lower())
    
    mail_text = mail_template(
        to_name = to_name,
        to_address = to_address,
        state_id = state_id,
        state_title = state_title,
        closing_date = closing_date,
        delivery_date = delivery_date,
        faborder = context,
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


def sendSketchUpdateMail(context, justReturn=False):
    """Sends notification when a sketch file has been replaced
    """
    mail_text = u""
    charset = 'utf-8'
    
    portal = getSite()
    mail_template = portal.mail_sketch_update
    sketch = context
    faborder = context.aq_parent
    faborders = faborder.aq_parent
    
    from_address = faborders.salesEmail
    from_name = "Fritzing Fab"
    to_address = faborder.email
    closing_date = faborders.nextProductionClosingDate
    user  = faborder.getOwner()
    to_name = user.getProperty('fullname', '')
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