from django import template

register = template.Library()

@register.simple_tag
def get_recent_posts(timeout=None):
    """
    Usage: {% get_recent_posts [timeout] %}

    Examples:
    {% get_recent_posts %}
    {% get_recent_posts 1 %}
    """
    import socket
    from urllib2 import urlopen
    socket_default_timeout = socket.getdefaulttimeout()
    if timeout is not None:
        try:
            socket_timeout = float(timeout)
        except ValueError:
            raise template.TemplateSyntaxError, "timeout argument of geturl tag, if provided, must be convertible to a float"
        try:
            socket.setdefaulttimeout(socket_timeout)
        except ValueError:
            raise template.TemplateSyntaxError, "timeout argument of geturl tag, if provided, cannot be less than zero"
    try:
        try:
            content = urlopen("http://blog.fritzing.org/recent-posts").read()
        finally: # reset socket timeout
            if timeout is not None:
                socket.setdefaulttimeout(socket_default_timeout)
    except:
        content = ''
    return content
