import string
import random
import csv
import logging
logger = logging.getLogger("Plone")
from urllib import urlencode
from urllib2 import urlopen, Request
from DateTime import DateTime
from StringIO import StringIO
import zipfile
try:
    import zlib
    compression = zipfile.ZIP_DEFLATED
except:
    compression = zipfile.ZIP_STORED

from five import grok
from Acquisition import aq_inner

from plone.directives import dexterity

from plone.memoize.instance import memoize
from Products.CMFCore.utils import getToolByName

from fritzing.fab.interfaces import IFabOrders, IFabOrder
from fritzing.fab.tools import sendStatusMail
from fritzing.fab import _


class Index(grok.View):
    grok.context(IFabOrders)
    message = None
    
    label = _(u"Fritzing Fab")
    description = _(u"There's nothing better than turning a concept into product reality.")
    
    # make tools availible to the template as view.toolname()
    from fritzing.fab.tools \
        import encodeFilename, getStateId, getStateTitle, isStateId, canDelete
    
    def update(self):
        self.member = self.context.portal_membership.getAuthenticatedMember()
        self.isManager = self.member.has_role('Manager')
        self.isOwner = self.member.has_role('Owner')
        self.hasOrders = len(self.listOrders()) > 0
        if not (self.isManager):
            self.request.set('disable_border', 1)

    @memoize
    def listOrders(self):
        """list orders visible for this user
        """
        catalog = getToolByName(self.context, 'portal_catalog')

        query = {
            'object_provides': IFabOrder.__identifier__,
            'path': dict(query='/'.join(self.context.getPhysicalPath()),
                         depth=1),
            'sort_on': 'Date',
            'Creator': self.member.id, }

        return [orderBrain.getObject() for orderBrain in catalog(query)]

    @memoize
    def listOrdersManager(self):
        """list orders visible for this user
        """
        catalog = getToolByName(self.context, 'portal_catalog')

        query = {
            'object_provides': IFabOrder.__identifier__,
            'path': dict(query='/'.join(self.context.getPhysicalPath()),
                         depth=1),
            'sort_on': 'Date',
            'review_state': 'in_process', }

        return [orderBrain.getObject() for orderBrain in catalog(query)]



class CurrentOrders(grok.View):
    """Overview of current orders
    """
    grok.name('currentorders')
    grok.require('cmf.ModifyPortalContent')
    grok.context(IFabOrders)
    
    # make tools availible to the template as view.toolname()
    from fritzing.fab.tools \
        import getCurrentOrders, encodeFilename, getStateId, getStateTitle, isStateId

    def update(self):
        pass

    def getShippingCountry(self, value):
        return IFabOrder['shippingCountry'].vocabulary.getTerm(value).title     
        
    

class CurrentOrdersCSV(grok.View):
    """All current orders as CSV table
    """
    grok.name('currentorders-csv')
    grok.require('cmf.ModifyPortalContent')
    grok.context(IFabOrders)
    
    # make tools availible to the template as view.toolname()
    from fritzing.fab.tools \
        import getCurrentOrders, encodeFilename

    def update(self):
        pass
    
    def render(self):
        """All current orders as CSV table
        """
        out = StringIO()
        context = aq_inner(self.context)
        writer = csv.writer(out)
        
        # Header
        writer.writerow(('date', 'order-id', 'filename', 'count', 'optional', 'boards', 'size', 'price', 'paid', 'checked', 'sent', 'invoiced', 'bloggable', 'name', 'e-mail', 'zone'))
        # Content
        for brain in self.getCurrentOrders(context):
            order = brain.getObject()
            for sketch in order.listFolderContents():
                writer.writerow((
                    DateTime(order.Date()).strftime('%y-%m-%d %H:%M:%S'), 
                    order.id,
                    order.id + "_" + self.encodeFilename(sketch.orderItem.filename),
                    sketch.copies,
                    "",
                    len(sketch.boards),
                    '%.2f' % (sketch.area * sketch.copies),
                    '%.2f' % (sketch.copies * sketch.area * order.pricePerSquareCm + 4.0),
                    "",
                    "",
                    "",
                    "",
                    "",
                    order.getOwner(),
                    order.email,
                    order.shipTo
                ))
        
        # Prepare response
        filename = "fab-orders-%s.csv" % context.Date()
        self.request.response.setHeader('Content-Type', 'text/csv')
        self.request.response.setHeader('Content-Disposition', 'attachment; filename="%s"' % filename)
        
        return out.getvalue()


class CurrentOrdersZIP(grok.View):
    """All current orders as ZIP file
    """
    grok.name('currentorders-zip')
    grok.require('cmf.ModifyPortalContent')
    grok.context(IFabOrders)
    
    # make tools availible to the template as view.toolname()
    from fritzing.fab.tools \
        import getCurrentOrders, encodeFilename

    def update(self):
        pass
    
    def render(self):
        """All current orders as ZIP file
        """
        out = StringIO()
        context = aq_inner(self.context)
        zipfilename = "fab-orders-%s.zip" % context.Date()
        
        zf = zipfile.ZipFile(
            out, 
            mode='w',
            compression = compression)
        try:
            for brain in self.getCurrentOrders(context):
                order = brain.getObject()
                for sketch in order.listFolderContents():
                    zf.writestr(
                        order.id + "_" + self.encodeFilename(sketch.orderItem.filename),
                        sketch.orderItem.data
                    )
        finally:
            zf.close()

        # Prepare response
        self.request.response.setHeader('Content-Type', 'application/octet-stream')
        self.request.response.setHeader('Content-Disposition', 'attachment; filename="%s"' % zipfilename)
        
        out.seek(0)
        content = out.read()
        out.close()
        return content


class PayPalIpn(grok.View):
    """Payment Confirmation
    """
    grok.name('paypal_ipn')
    grok.require('zope2.View')
    grok.context(IFabOrders)

    sample_ipn = "mc_gross=19.95&protection_eligibility=Eligible&address_status=confirmed&payer_id=LPLWNMTBWMFAY&tax=0.00&address_street=1+Main+St&payment_date=20%3A12%3A59+Jan+13%2C+2009+PST&payment_status=Completed&charset=windows-1252&address_zip=95131&first_name=Test&mc_fee=0.88&address_country_code=US&address_name=Test+User&notify_version=2.6&custom=&payer_status=verified&address_country=United+States&address_city=San+Jose&quantity=1&verify_sign=AtkOfCXbDm2hu0ZELryHFjY-Vb7PAUvS6nMXgysbElEn9v-1XcmSoGtf&payer_email=gpmac_1231902590_per%40paypal.com&txn_id=61E67681CH3238416&payment_type=instant&last_name=User&address_state=CA&receiver_email=order@ixds.de&payment_fee=0.88&receiver_id=S8XGHLYDW9T3S&txn_type=express_checkout&item_name=&mc_currency=USD&item_number=&residence_country=US&test_ipn=1&handling_amount=0.00&transaction_subject=&payment_gross=19.95&shipping=5.30"
    
    def verify_ipn(self, data):
        """ verify payment notification
        """
        faborders = aq_inner(self.context)

        # see https://www.x.com/developers/paypal/documentation-tools/ipn/integration-guide/IPNIntro
        # prepares provided data set to inform PayPal we wish to validate the response
        data["cmd"] = "_notify-validate"
        params = urlencode(data)
        # sends the data and request to the PayPal Sandbox
        req = Request("""https://www.sandbox.paypal.com/cgi-bin/webscr""", params)
        req.add_header("Content-type", "application/x-www-form-urlencoded")
        # reads the response back from PayPal
        response = urlopen(req)
        status = response.read()
     
        # verify validity
        error_msg = "could not verify PayPal IPN: " + str(data)
        if not status == "VERIFIED":
            return None, "Wrong status - " + error_msg
        if not data["receiver_email"].lower() == faborders.paypalEmail.lower():
            return None, "Wrong receiver_email - " + error_msg
        if not data["item_name"].lower() == "fritzing fab order":
            return None, "Wrong item_name - " + error_msg
        if not data["mc_currency"] == "EUR":
            return None, "Wrong mc_currency - " + error_msg

        # find associated order
        catalog = getToolByName(self.context, 'portal_catalog')
        query = {
            'id': data['item_number'],
            'object_provides': IFabOrder.__identifier__,
        }
        results = catalog.unrestrictedSearchResults(query)
        if len(results) <> 1:
            return None, "Could not find fab order - " + error_msg
        faborder = results[0]._unrestrictedGetObject()

        return faborder, "PayPal IPN verified"


    def process_ipn(self, faborder, data):
        """ process payment
        """

        # check payment status
        if not data["payment_status"].lower() == "completed":
            # TODO: check for pending_reason
            msg = "Payment incomplete: status " + data["payment_status"]
            faborder.paymentMsg = msg
            return False, msg

        # check that paymentAmount is correct
        if faborder.priceTotalBrutto <> data['mc_gross']:
            msg = "Payment sum does not match: Received " + data['mc_gross'] + \
                " instead of " + faborder.priceTotalBrutto
            faborder.paymentMsg = msg
            return False, msg

        # update order
        if data['txn_id'] == faborder.paymentId:
            return True, "Payment already processed"
        else:
            faborder.paymentId = data['txn_id']

        portal_workflow = getToolByName(self, 'portal_workflow')
        review_state = portal_workflow.getInfoFor(faborder, 'review_state')
        if review_state <> 'open':
            return True, "Order is already " + review_state
        # Change workflow state.  Don't use portal_workflow.doActionFor but
        # portal_workflow._invokeWithNotification to avoid security checks:
        wfs = portal_workflow.getWorkflowsFor(faborder)
        wf = wfs[0]
        portal_workflow._invokeWithNotification(wfs, faborder, 'submit', wf._changeStateOf, 
            (faborder, wf.transitions.get('submit')), {})
        faborder.reindexObjectSecurity()
        review_state = portal_workflow.getInfoFor(faborder, 'review_state')
        if review_state <> 'in_process':
            msg = "Could not set order status from " + faborder.review_state + " to in_process"
            faborder.paymentMsg = msg
            return False, msg

        faborder.paymentMsg = "PayPal transaction " + data['txn_id'] + " successfully processed"
        return True, msg        


    def update(self):
        data = self.request.form

        faborder, msg = self.verify_ipn(data)
        if not faborder:
            logger.error(msg)
            return msg
        
        processed, msg = self.process_ipn(faborder, data)
        if not processed:
            logger.error(msg)
            return msg
        else:
            logger.info(msg)
            return msg

    
    def render(self):
        return "PAYPAL IPN"


class AddForm(dexterity.AddForm):
    """creates a new faborder transparently
    """
    grok.name('faborder')
    grok.require('cmf.AddPortalContent')
    grok.context(IFabOrders)
    
    schema = IFabOrder
    
    label = _(u"New Fab Order")
    description = _(u"Creates a new order for the Fritzing Fab Service")
    
    def create(self, data):
        from zope.component import createObject
        object = createObject('faborder')
        object.id = data['id']
        object.title = data['name']
        user = self.context.portal_membership.getAuthenticatedMember()
        object.email = user.getProperty('email')
        object.exclude_from_nav = True
        object.reindexObject()
        return object
    
    def add(self, object):
        self.context._setObject(object.id, object)
        o = getattr(self.context, object.id)
        o.reindexObject()

    def render(self):
        """create faborder instance and redirect to its default view
        """
        
        # generate a nice order-number
        length = 8 # order number length
        chars = list(string.digits) # possible chars in order numbers
        # chars = chars, list(string.ascii_lowercase) # (add letters)
        # chars = sum(chars, []) # (flatten list)
        n = "".join(random.sample(chars, length))
        while self.context.hasObject(n):
            n = "".join(random.sample(chars, length))
        
        instance = self.create({
            'id': n, 
            'name': u"Fritzing Fab order %s" % (n)})
        self.add(instance)
        
        faborderURL = self.context.absolute_url()+"/"+instance.id
        self.request.response.redirect(faborderURL)

