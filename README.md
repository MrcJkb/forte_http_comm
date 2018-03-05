# FORTE HTTP Com Layer
Simple HTTP Com Layer for 4diac-RTE (FORTE).

# This repository is no longer maintained!
It has been migrated to the official FORTE project. There will be no further development on this repository. As long as it has not yet been published to the stable version of FORTE, you can still use this repository or download/pull the develop branch of FORTE for the latest version of this module.
​
# Requires
* 4diac-RTE (https://www.eclipse.org/4diac/)

# Installation
* Add the folder containing the source files to the FORTE modules directory (src/modules/)
* In CMake GUI, enable FORTE_COM_HTTP
​
# Documentation
* Currently, only HTTP GET and PUT requests are supported.
* For GET requests, use a CLIENT_0_1 function block.
* For PUT requests, use a CLIENT_1_0 or CLIENT_1 function block
* Other function blocks are currently not supported and may result in unexpected behaviour.

# Parameters
http[ip:port/path] <br />
http[ip:port/path;expected_response_code]
* ip: The IP address
* port: The port number
* path: The path in the URL
* expected_response_code (optional): The expected HTTP response code (= "HTTP/1.1 200 OK" if none is specified)

examples: <br />
http[144.12.131.2:80/rest/battery/voltage] <br />
http[144.12.131.2:80/rest/battery/voltage;HTTP/1.1 201 Created]

# Notes
* The project is still in its early development stages. The interface is subject to change.
* A CHttpParser class is used to parse the HTTP requests and responses in the communication layer.
  It was designed with the goal of requesting measurements and setting values via a REST server.
  Thus, it is assumed that a number is returned for the GET request and a number is sent for the PUT request.
  To adjust the parsing of requests and responses, change the CHttpParser class accordingly.
* In the long run, a factory for the CHttpParser may be implemented.
* The following IEC 61499 data types are currently NOT supported for PUT requests: DATE, DATE_AND_TIME, TIME_OF_DAY, arrays, structs
* By default, a GET request is sent as: <br />
  GET /path HTTP/1.1 <br />
  Host: ip:port" <br />
  <br />
* By default, a PUT request is sent as: <br />
  PUT /path HTTP/1.1 <br />
  Host: ip:port <br />
  Content-type: text/html <br />
  Content-length: length_of_data_string <br />
  <br />
  <br />
  data_string
* If the CLIENT function block has a data output, the HTTP response code (e.g., "HTTP/1.1 404 Not found") is output
  along with a "SEND_FAILED" status output in the case of an unexpected response.
  The default expected response code is "HTTP/1.1 200 OK".
