#!/usr/bin/env python3
from lxml import html
from lxml import etree
import sys, os, requests, argparse, time, socket

def retrieve_debug_logs(hostip, outdirectory):
    try:
        host = socket.gethostbyaddr(hostip)[0]
    except (socket.gaierror):
        print("could not resolve hostname: %s, error:%s" %(hostip))
        sys.exit(-1)

    page = requests.get("http://%s/CGI/Java/Serviceability?adapter=device.statistics.consolelog" %(host))
    tree = html.fromstring(page.content)
    for a in tree.xpath('//div/table/tr/td/b/a'):
        href = a.get('href')
        filename = os.path.basename(href)
        filename = os.path.splitext(filename)[0]
        if (filename == 'fsck.fd0a' or filename == 'fsck.fd1a'):
            continue

        timestr = time.strftime("%Y%m%d-%H%M%S")
        file = '%s/%s_%s_%s.log' %(outdirectory, host, filename, timestr)
        url = "http://%s/%s" %(host, href)
        print('retrieving:%s, storing:%s' %(url, file))
        debug_log = requests.get(url)
        with open(file, 'wb') as f:
            for chunk in debug_log.iter_content(chunk_size=1024): 
                if chunk:
                    f.write(chunk)
    return (tree)

def main(argv):
    parser = argparse.ArgumentParser(description='Retrieve Debug Logs from SCCP Phone.')
    parser.add_argument('-i', '--ipaddress', dest='ipaddress', default='', nargs='?', required=True, help='ip-address or hostname')
    parser.add_argument('-d', '--directory', dest='outdirectory', default='/tmp', nargs='?', help='output directory')
    args = parser.parse_args()

    if not os.path.exists(args.outdirectory):
        print("creating output directory ",args.outdirectory)
        os.makedirs(args.outdirectory)

    try:
        print('ip-address: %s, output directory:%s' %(hostip, outdirectory))    
        retrieve_debug_logs(agrs.ipaddress, args.outdirectory)
    except (Exception):
        print('SCCP phone could not be reached');
        sys.exit(-1)

    sys.exit(0)
    
if __name__ == "__main__":
    main(sys.argv[1:])

    