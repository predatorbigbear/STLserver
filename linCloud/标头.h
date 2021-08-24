#pragma once

#include<string_view>
#include<unordered_map>


static std::unordered_map<std::string_view, std::string_view> contentTypeMap{
{std::string_view(".*"),std::string_view("application/octet-stream")},
{std::string_view(".tif"),std::string_view("image/tiff")},
{std::string_view(".001"),std::string_view("application/x-001")},
{std::string_view(".301"),std::string_view("application/x-301")},
{std::string_view(".323"),std::string_view("text/h323")},
{std::string_view(".906"),std::string_view("application/x-906")},
{std::string_view(".907"),std::string_view("drawing/907")},
{std::string_view(".a11"),std::string_view("application/x-a11")},
{std::string_view(".acp"),std::string_view("audio/x-mei-aac")},
{std::string_view(".ai"),std::string_view("application/postscript")},
{std::string_view(".aif"),std::string_view("audio/aiff")},
{std::string_view(".aifc"),std::string_view("audio/aiff")},
{std::string_view(".aiff"),std::string_view("audio/aiff")},
{std::string_view(".anv"),std::string_view("application/x-anv")},
{std::string_view(".asa"),std::string_view("text/asa")},
{std::string_view(".asf"),std::string_view("video/x-ms-asf")},
{std::string_view(".asp"),std::string_view("text/asp")},
{std::string_view(".asx"),std::string_view("video/x-ms-asf")},
{std::string_view(".au"),std::string_view("audio/basic")},
{std::string_view(".avi"),std::string_view("video/avi")},
{std::string_view(".awf"),std::string_view("application/vnd.adobe.workflow")},
{std::string_view(".biz"),std::string_view("text/xml")},
{std::string_view(".bmp"),std::string_view("application/x-bmp")},
{std::string_view(".bot"),std::string_view("application/x-bot")},
{std::string_view(".c4t"),std::string_view("application/x-c4t")},
{std::string_view(".c90"),std::string_view("application/x-c90")},
{std::string_view(".cal"),std::string_view("application/x-cals")},
{std::string_view(".cat"),std::string_view("application/vnd.ms-pki.seccat")},
{std::string_view(".cdf"),std::string_view("application/x-netcdf")},
{std::string_view(".cdr"),std::string_view("application/x-cdr")},
{std::string_view(".cel"),std::string_view("application/x-cel")},
{std::string_view(".cer"),std::string_view("application/x-x509-ca-cert")},
{std::string_view(".cg4"),std::string_view("application/x-g4")},
{std::string_view(".cgm"),std::string_view("application/x-cgm")},
{std::string_view(".cit"),std::string_view("application/x-cit")},
{std::string_view(".class"),std::string_view("java/*")},
{std::string_view(".cml"),std::string_view("text/xml")},
{std::string_view(".cmp"),std::string_view("application/x-cmp")},
{std::string_view(".cmx"),std::string_view("application/x-cmx")},
{std::string_view(".cot"),std::string_view("application/x-cot")},
{std::string_view(".crl"),std::string_view("application/pkix-crl")},
{std::string_view(".crt"),std::string_view("application/x-x509-ca-cert")},
{std::string_view(".csi"),std::string_view("application/x-csi")},
{std::string_view(".css"),std::string_view("text/css")},
{std::string_view(".cut"),std::string_view("application/x-cut")},
{std::string_view(".dbf"),std::string_view("application/x-dbf")},
{std::string_view(".dbm"),std::string_view("application/x-dbm")},
{std::string_view(".dbx"),std::string_view("application/x-dbx")},
{std::string_view(".dcd"),std::string_view("text/xml")},
{std::string_view(".dcx"),std::string_view("application/x-dcx")},
{std::string_view(".der"),std::string_view("application/x-x509-ca-cert")},
{std::string_view(".dgn"),std::string_view("application/x-dgn")},
{std::string_view(".dib"),std::string_view("application/x-dib")},
{std::string_view(".dll"),std::string_view("application/x-msdownload")},
{std::string_view(".doc"),std::string_view("application/msword")},
{std::string_view(".dot"),std::string_view("application/msword")},
{std::string_view(".drw"),std::string_view("application/x-drw")},
{std::string_view(".dtd"),std::string_view("text/xml")},
{std::string_view(".dwf"),std::string_view("Model/vnd.dwf")},
{std::string_view(".dwf"),std::string_view("application/x-dwf")},
{std::string_view(".dwg"),std::string_view("application/x-dwg")},
{std::string_view(".dxb"),std::string_view("application/x-dxb")},
{std::string_view(".dxf"),std::string_view("application/x-dxf")},
{std::string_view(".edn"),std::string_view("application/vnd.adobe.edn")},
{std::string_view(".emf"),std::string_view("application/x-emf")},
{std::string_view(".eml"),std::string_view("message/rfc822")},
{std::string_view(".ent"),std::string_view("text/xml")},
{std::string_view(".epi"),std::string_view("application/x-epi")},
{std::string_view(".eps"),std::string_view("application/x-ps")},
{std::string_view(".eps"),std::string_view("application/postscript")},
{std::string_view(".etd"),std::string_view("application/x-ebx")},
{std::string_view(".exe"),std::string_view("application/x-msdownload")},
{std::string_view(".fax"),std::string_view("image/fax")},
{std::string_view(".fdf"),std::string_view("application/vnd.fdf")},
{std::string_view(".fif"),std::string_view("application/fractals")},
{std::string_view(".fo"),std::string_view("text/xml")},
{std::string_view(".frm"),std::string_view("application/x-frm")},
{std::string_view(".g4"),std::string_view("application/x-g4")},
{std::string_view(".gbr"),std::string_view("application/x-gbr")},
{std::string_view("."),std::string_view("application/x-")},
{std::string_view(".gif"),std::string_view("image/gif")},
{std::string_view(".gl2"),std::string_view("application/x-gl2")},
{std::string_view(".gp4"),std::string_view("application/x-gp4")},
{std::string_view(".hgl"),std::string_view("application/x-hgl")},
{std::string_view(".hmr"),std::string_view("application/x-hmr")},
{std::string_view(".hpg"),std::string_view("application/x-hpgl")},
{std::string_view(".hpl"),std::string_view("application/x-hpl")},
{std::string_view(".hqx"),std::string_view("application/mac-binhex40")},
{std::string_view(".hrf"),std::string_view("application/x-hrf")},
{std::string_view(".hta"),std::string_view("application/hta")},
{std::string_view(".htc"),std::string_view("text/x-component")},
{std::string_view(".htm"),std::string_view("text/html")},
{std::string_view(".html"),std::string_view("text/html")},
{std::string_view(".htt"),std::string_view("text/webviewhtml")},
{std::string_view(".htx"),std::string_view("text/html")},
{std::string_view(".icb"),std::string_view("application/x-icb")},
{std::string_view(".ico"),std::string_view("image/x-icon")},
{std::string_view(".ico"),std::string_view("application/x-ico")},
{std::string_view(".iff"),std::string_view("application/x-iff")},
{std::string_view(".ig4"),std::string_view("application/x-g4")},
{std::string_view(".igs"),std::string_view("application/x-igs")},
{std::string_view(".iii"),std::string_view("application/x-iphone")},
{std::string_view(".img"),std::string_view("application/x-img")},
{std::string_view(".ins"),std::string_view("application/x-internet-signup")},
{std::string_view(".isp"),std::string_view("application/x-internet-signup")},
{std::string_view(".IVF"),std::string_view("video/x-ivf")},
{std::string_view(".java"),std::string_view("java/*")},
{std::string_view(".jfif"),std::string_view("image/jpeg")},
{std::string_view(".jpe"),std::string_view("image/jpeg")},
{std::string_view(".jpe"),std::string_view("application/x-jpe")},
{std::string_view(".jpeg"),std::string_view("image/jpeg")},
{std::string_view(".jpg"),std::string_view("image/jpeg")},
{std::string_view(".jpg"),std::string_view("application/x-jpg")},
{std::string_view(".js"),std::string_view("application/x-javascript")},
{std::string_view(".jsp"),std::string_view("text/html")},
{std::string_view(".la1"),std::string_view("audio/x-liquid-file")},
{std::string_view(".lar"),std::string_view("application/x-laplayer-reg")},
{std::string_view(".latex"),std::string_view("application/x-latex")},
{std::string_view(".lavs"),std::string_view("audio/x-liquid-secure")},
{std::string_view(".lbm"),std::string_view("application/x-lbm")},
{std::string_view(".lmsff"),std::string_view("audio/x-la-lms")},
{std::string_view(".ls"),std::string_view("application/x-javascript")},
{std::string_view(".ltr"),std::string_view("application/x-ltr")},
{std::string_view(".m1v"),std::string_view("video/x-mpeg")},
{std::string_view(".m2v"),std::string_view("video/x-mpeg")},
{std::string_view(".m3u"),std::string_view("audio/mpegurl")},
{std::string_view(".m4e"),std::string_view("video/mpeg4")},
{std::string_view(".mac"),std::string_view("application/x-mac")},
{std::string_view(".man"),std::string_view("application/x-troff-man")},
{std::string_view(".math"),std::string_view("text/xml")},
{std::string_view(".mdb"),std::string_view("application/msaccess")},
{std::string_view(".mdb"),std::string_view("application/x-mdb")},
{std::string_view(".mfp"),std::string_view("application/x-shockwave-flash")},
{std::string_view(".mht"),std::string_view("message/rfc822")},
{std::string_view(".mhtml"),std::string_view("message/rfc822")},
{std::string_view(".mi"),std::string_view("application/x-mi")},
{std::string_view(".mid"),std::string_view("audio/mid")},
{std::string_view(".midi"),std::string_view("audio/mid")},
{std::string_view(".mil"),std::string_view("application/x-mil")},
{std::string_view(".mml"),std::string_view("text/xml")},
{std::string_view(".mnd"),std::string_view("audio/x-musicnet-download")},
{std::string_view(".mns"),std::string_view("audio/x-musicnet-stream")},
{std::string_view(".mocha"),std::string_view("application/x-javascript")},
{std::string_view(".movie"),std::string_view("video/x-sgi-movie")},
{std::string_view(".mp1"),std::string_view("audio/mp1")},
{std::string_view(".mp2"),std::string_view("audio/mp2")},
{std::string_view(".mp2v"),std::string_view("video/mpeg")},
{std::string_view(".mp3"),std::string_view("audio/mp3")},
{std::string_view(".mp4"),std::string_view("video/mpeg4")},
{std::string_view(".mpa"),std::string_view("video/x-mpg")},
{std::string_view(".mpd"),std::string_view("application/vnd.ms-project")},
{std::string_view(".mpe"),std::string_view("video/x-mpeg")},
{std::string_view(".mpeg"),std::string_view("video/mpg")},
{std::string_view(".mpg"),std::string_view("video/mpg")},
{std::string_view(".mpga"),std::string_view("audio/rn-mpeg")},
{std::string_view(".mpp"),std::string_view("application/vnd.ms-project")},
{std::string_view(".mps"),std::string_view("video/x-mpeg")},
{std::string_view(".mpt"),std::string_view("application/vnd.ms-project")},
{std::string_view(".mpv"),std::string_view("video/mpg")},
{std::string_view(".mpv2"),std::string_view("video/mpeg")},
{std::string_view(".mpw"),std::string_view("application/vnd.ms-project")},
{std::string_view(".mpx"),std::string_view("application/vnd.ms-project")},
{std::string_view(".mtx"),std::string_view("text/xml")},
{std::string_view(".mxp"),std::string_view("application/x-mmxp")},
{std::string_view(".net"),std::string_view("image/pnetvue")},
{std::string_view(".nrf"),std::string_view("application/x-nrf")},
{std::string_view(".nws"),std::string_view("message/rfc822")},
{std::string_view(".odc"),std::string_view("text/x-ms-odc")},
{std::string_view(".out"),std::string_view("application/x-out")},
{std::string_view(".p10"),std::string_view("application/pkcs10")},
{std::string_view(".p12"),std::string_view("application/x-pkcs12")},
{std::string_view(".p7b"),std::string_view("application/x-pkcs7-certificates")},
{std::string_view(".p7c"),std::string_view("application/pkcs7-mime")},
{std::string_view(".p7m"),std::string_view("application/pkcs7-mime")},
{std::string_view(".p7r"),std::string_view("application/x-pkcs7-certreqresp")},
{std::string_view(".p7s"),std::string_view("application/pkcs7-signature")},
{std::string_view(".pc5"),std::string_view("application/x-pc5")},
{std::string_view(".pci"),std::string_view("application/x-pci")},
{std::string_view(".pcl"),std::string_view("application/x-pcl")},
{std::string_view(".pcx"),std::string_view("application/x-pcx")},
{std::string_view(".pdf"),std::string_view("application/pdf")},
{std::string_view(".pdf"),std::string_view("application/pdf")},
{std::string_view(".pdx"),std::string_view("application/vnd.adobe.pdx")},
{std::string_view(".pfx"),std::string_view("application/x-pkcs12")},
{std::string_view(".pgl"),std::string_view("application/x-pgl")},
{std::string_view(".pic"),std::string_view("application/x-pic")},
{std::string_view(".pko"),std::string_view("application/vnd.ms-pki.pko")},
{std::string_view(".pl"),std::string_view("application/x-perl")},
{std::string_view(".plg"),std::string_view("text/html")},
{std::string_view(".pls"),std::string_view("audio/scpls")},
{std::string_view(".plt"),std::string_view("application/x-plt")},
{std::string_view(".png"),std::string_view("image/png")},
{std::string_view(".png"),std::string_view("application/x-png")},
{std::string_view(".pot"),std::string_view("application/vnd.ms-powerpoint")},
{std::string_view(".ppa"),std::string_view("application/vnd.ms-powerpoint")},
{std::string_view(".ppm"),std::string_view("application/x-ppm")},
{std::string_view(".pps"),std::string_view("application/vnd.ms-powerpoint")},
{std::string_view(".ppt"),std::string_view("application/vnd.ms-powerpoint")},
{std::string_view(".ppt"),std::string_view("application/x-ppt")},
{std::string_view(".pr"),std::string_view("application/x-pr")},
{std::string_view(".prf"),std::string_view("application/pics-rules")},
{std::string_view(".prn"),std::string_view("application/x-prn")},
{std::string_view(".prt"),std::string_view("application/x-prt")},
{std::string_view(".ps"),std::string_view("application/x-ps")},
{std::string_view(".ps"),std::string_view("application/postscript")},
{std::string_view(".ptn"),std::string_view("application/x-ptn")},
{std::string_view(".pwz"),std::string_view("application/vnd.ms-powerpoint")},
{std::string_view(".r3t"),std::string_view("text/vnd.rn-realtext3d")},
{std::string_view(".ra"),std::string_view("audio/vnd.rn-realaudio")},
{std::string_view(".ram"),std::string_view("audio/x-pn-realaudio")},
{std::string_view(".ras"),std::string_view("application/x-ras")},
{std::string_view(".rat"),std::string_view("application/rat-file")},
{std::string_view(".rdf"),std::string_view("text/xml")},
{std::string_view(".rec"),std::string_view("application/vnd.rn-recording")},
{std::string_view(".red"),std::string_view("application/x-red")},
{std::string_view(".rgb"),std::string_view("application/x-rgb")},
{std::string_view(".rjs"),std::string_view("application/vnd.rn-realsystem-rjs")},
{std::string_view(".rjt"),std::string_view("application/vnd.rn-realsystem-rjt")},
{std::string_view(".rlc"),std::string_view("application/x-rlc")},
{std::string_view(".rle"),std::string_view("application/x-rle")},
{std::string_view(".rm"),std::string_view("application/vnd.rn-realmedia")},
{std::string_view(".rmf"),std::string_view("application/vnd.adobe.rmf")},
{std::string_view(".rmi"),std::string_view("audio/mid")},
{std::string_view(".rmj"),std::string_view("application/vnd.rn-realsystem-rmj")},
{std::string_view(".rmm"),std::string_view("audio/x-pn-realaudio")},
{std::string_view(".rmp"),std::string_view("application/vnd.rn-rn_music_package")},
{std::string_view(".rms"),std::string_view("application/vnd.rn-realmedia-secure")},
{std::string_view(".rmvb"),std::string_view("application/vnd.rn-realmedia-vbr")},
{std::string_view(".rmx"),std::string_view("application/vnd.rn-realsystem-rmx")},
{std::string_view(".rnx"),std::string_view("application/vnd.rn-realplayer")},
{std::string_view(".rp"),std::string_view("image/vnd.rn-realpix")},
{std::string_view(".rpm"),std::string_view("audio/x-pn-realaudio-plugin")},
{std::string_view(".rsml"),std::string_view("application/vnd.rn-rsml")},
{std::string_view(".rt"),std::string_view("text/vnd.rn-realtext")},
{std::string_view(".rtf"),std::string_view("application/msword")},
{std::string_view(".rtf"),std::string_view("application/x-rtf")},
{std::string_view(".rv"),std::string_view("video/vnd.rn-realvideo")},
{std::string_view(".sam"),std::string_view("application/x-sam")},
{std::string_view(".sat"),std::string_view("application/x-sat")},
{std::string_view(".sdp"),std::string_view("application/sdp")},
{std::string_view(".sdw"),std::string_view("application/x-sdw")},
{std::string_view(".sit"),std::string_view("application/x-stuffit")},
{std::string_view(".slb"),std::string_view("application/x-slb")},
{std::string_view(".sld"),std::string_view("application/x-sld")},
{std::string_view(".slk"),std::string_view("drawing/x-slk")},
{std::string_view(".smi"),std::string_view("application/smil")},
{std::string_view(".smil"),std::string_view("application/smil")},
{std::string_view(".smk"),std::string_view("application/x-smk")},
{std::string_view(".snd"),std::string_view("audio/basic")},
{std::string_view(".sol"),std::string_view("text/plain")},
{std::string_view(".sor"),std::string_view("text/plain")},
{std::string_view(".spc"),std::string_view("application/x-pkcs7-certificates")},
{std::string_view(".spl"),std::string_view("application/futuresplash")},
{std::string_view(".spp"),std::string_view("text/xml")},
{std::string_view(".ssm"),std::string_view("application/streamingmedia")},
{std::string_view(".sst"),std::string_view("application/vnd.ms-pki.certstore")},
{std::string_view(".stl"),std::string_view("application/vnd.ms-pki.stl")},
{std::string_view(".stm"),std::string_view("text/html")},
{std::string_view(".sty"),std::string_view("application/x-sty")},
{std::string_view(".svg"),std::string_view("text/xml")},
{std::string_view(".swf"),std::string_view("application/x-shockwave-flash")},
{std::string_view(".tdf"),std::string_view("application/x-tdf")},
{std::string_view(".tg4"),std::string_view("application/x-tg4")},
{std::string_view(".tga"),std::string_view("application/x-tga")},
{std::string_view(".tif"),std::string_view("image/tiff")},
{std::string_view(".tif"),std::string_view("application/x-tif")},
{std::string_view(".tiff"),std::string_view("image/tiff")},
{std::string_view(".tld"),std::string_view("text/xml")},
{std::string_view(".top"),std::string_view("drawing/x-top")},
{std::string_view(".torrent"),std::string_view("application/x-bittorrent")},
{std::string_view(".tsd"),std::string_view("text/xml")},
{std::string_view(".txt"),std::string_view("text/plain")},
{std::string_view(".uin"),std::string_view("application/x-icq")},
{std::string_view(".uls"),std::string_view("text/iuls")},
{std::string_view(".vcf"),std::string_view("text/x-vcard")},
{std::string_view(".vda"),std::string_view("application/x-vda")},
{std::string_view(".vdx"),std::string_view("application/vnd.visio")},
{std::string_view(".vml"),std::string_view("text/xml")},
{std::string_view(".vpg"),std::string_view("application/x-vpeg005")},
{std::string_view(".vsd"),std::string_view("application/vnd.visio")},
{std::string_view(".vsd"),std::string_view("application/x-vsd")},
{std::string_view(".vss"),std::string_view("application/vnd.visio")},
{std::string_view(".vst"),std::string_view("application/vnd.visio")},
{std::string_view(".vst"),std::string_view("application/x-vst")},
{std::string_view(".vsw"),std::string_view("application/vnd.visio")},
{std::string_view(".vsx"),std::string_view("application/vnd.visio")},
{std::string_view(".vtx"),std::string_view("application/vnd.visio")},
{std::string_view(".vxml"),std::string_view("text/xml")},
{std::string_view(".wav"),std::string_view("audio/wav")},
{std::string_view(".wax"),std::string_view("audio/x-ms-wax")},
{std::string_view(".wb1"),std::string_view("application/x-wb1")},
{std::string_view(".wb2"),std::string_view("application/x-wb2")},
{std::string_view(".wb3"),std::string_view("application/x-wb3")},
{std::string_view(".wbmp"),std::string_view("image/vnd.wap.wbmp")},
{std::string_view(".wiz"),std::string_view("application/msword")},
{std::string_view(".wk3"),std::string_view("application/x-wk3")},
{std::string_view(".wk4"),std::string_view("application/x-wk4")},
{std::string_view(".wkq"),std::string_view("application/x-wkq")},
{std::string_view(".wks"),std::string_view("application/x-wks")},
{std::string_view(".wm"),std::string_view("video/x-ms-wm")},
{std::string_view(".wma"),std::string_view("audio/x-ms-wma")},
{std::string_view(".wmd"),std::string_view("application/x-ms-wmd")},
{std::string_view(".wmf"),std::string_view("application/x-wmf")},
{std::string_view(".wml"),std::string_view("text/vnd.wap.wml")},
{std::string_view(".wmv"),std::string_view("video/x-ms-wmv")},
{std::string_view(".wmx"),std::string_view("video/x-ms-wmx")},
{std::string_view(".wmz"),std::string_view("application/x-ms-wmz")},
{std::string_view(".wp6"),std::string_view("application/x-wp6")},
{std::string_view(".wpd"),std::string_view("application/x-wpd")},
{std::string_view(".wpg"),std::string_view("application/x-wpg")},
{std::string_view(".wpl"),std::string_view("application/vnd.ms-wpl")},
{std::string_view(".wq1"),std::string_view("application/x-wq1")},
{std::string_view(".wr1"),std::string_view("application/x-wr1")},
{std::string_view(".wri"),std::string_view("application/x-wri")},
{std::string_view(".wrk"),std::string_view("application/x-wrk")},
{std::string_view(".ws"),std::string_view("application/x-ws")},
{std::string_view(".ws2"),std::string_view("application/x-ws")},
{std::string_view(".wsc"),std::string_view("text/scriptlet")},
{std::string_view(".wsdl"),std::string_view("text/xml")},
{std::string_view(".wvx"),std::string_view("video/x-ms-wvx")},
{std::string_view(".xdp"),std::string_view("application/vnd.adobe.xdp")},
{std::string_view(".xdr"),std::string_view("text/xml")},
{std::string_view(".xfd"),std::string_view("application/vnd.adobe.xfd")},
{std::string_view(".xfdf"),std::string_view("application/vnd.adobe.xfdf")},
{std::string_view(".xhtml"),std::string_view("text/html")},
{std::string_view(".xls"),std::string_view("application/vnd.ms-excel")},
{std::string_view(".xls"),std::string_view("application/x-xls")},
{std::string_view(".xlw"),std::string_view("application/x-xlw")},
{std::string_view(".xml"),std::string_view("text/xml")},
{std::string_view(".xpl"),std::string_view("audio/scpls")},
{std::string_view(".xq"),std::string_view("text/xml")},
{std::string_view(".xql"),std::string_view("text/xml")},
{std::string_view(".xquery"),std::string_view("text/xml")},
{std::string_view(".xsd"),std::string_view("text/xml")},
{std::string_view(".xsl"),std::string_view("text/xml")},
{std::string_view(".xslt"),std::string_view("text/xml")},
{std::string_view(".xwd"),std::string_view("application/x-xwd")},
{std::string_view(".x_b"),std::string_view("application/x-x_b")},
{std::string_view(".sis"),std::string_view("application/vnd.symbian.install")},
{std::string_view(".sisx"),std::string_view("application/vnd.symbian.install")},
{std::string_view(".x_t"),std::string_view("application/x-x_t")},
{std::string_view(".ipa"),std::string_view("application/vnd.iphone")},
{std::string_view(".apk"),std::string_view("application/vnd.android.package-archive")},
{std::string_view(".xap"),std::string_view("application/x-silverlight-app")} 

};