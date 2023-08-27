proc str_replace {orig rep with} {
    return [string map [list $rep $with] $orig]
}

# TODO rename this to lowercase underscore
proc generateDirectoryListingHtml {dirPath} {
    set fileList [lsort [concat [list ..] [glob -nocomplain -directory $dirPath *]]]
    set html "<!DOCTYPE html>\n<html>\n<head>\n<title>Index of: $dirPath</title>\n<style>table {border-collapse: collapse;} th, td {border: 1px solid black; padding: 8px; text-align: left;}</style>\n</head>\n<body>\n<h1>Index of: $dirPath</h1>\n<table>\n<tr><th>Name</th><th>Type</th><th>Size</th></tr>\n"

    foreach file $fileList {
        set fileName [file tail $file]
        set fileType [file type $file]

        if { $fileType eq "file" } {
            set fileSize [file size $file]
            append html "<tr><td><a href=\"/$dirPath/$fileName\">$fileName</a></td><td>$fileType</td><td>$fileSize bytes</td></tr>\n"
        } elseif { $fileType eq "directory" } {
            append html "<tr><td><a href=\"/$dirPath/$fileName\">$fileName</a></td><td>$fileType</td><td>-</td></tr>\n"
        }
    }

    append html "</table>\n</body>\n</html>"
    return $html
}

proc get_utc_dt {} {
    # Get current time in UTC (seconds since the Unix epoch)
    set utc_time [clock seconds]

    # Format the UTC time as a human-readable string
    set utc_datetime [clock format $utc_time -format "%Y-%m-%d %H:%M:%S" -gmt true]

    return $utc_datetime
}

proc get_file_suffix {filepath} {
    return [string trimleft [file extension $filepath] "."]
}

proc get_content_type {suffix} {
    array set contentTypes {
        html "text/html"
        htm "text/html"
        css "text/css"
        js "application/javascript"
        txt "text/plain"
        c "text/plain"
        y "text/plain"
        tar "application/octet-stream"
        m4a "audio/mp4"
	png "image/png"
	svg "image/svg+xml"
    }

    if {[info exists contentTypes($suffix)]} {
        return $contentTypes($suffix)
    }

    # Add more file types as needed
    return "text/plain"
    # return "application/octet-stream"; # Default MIME type for unknown file types
}

proc gen_response_content {filepath} {
    if {$filepath eq ""} {
        set filepath "templates/index.html"
    }
    
    if {[file isdirectory $filepath]} {
	set content_type "text/html"
        set content [generateDirectoryListingHtml $filepath]
    } elseif {[file isfile $filepath]} {
	set content_type [get_content_type [get_file_suffix $filepath]]
	if {[string match "text/*" $content_type]} {
	    set f [open $filepath r]
	    fconfigure $f -encoding utf-8
	    set content [read $f]
	    close $f
	} else {
	    set content ""
	}
        
        if {[string match "templates/*" $filepath]} {
            set utc_dt [get_utc_dt]
            set content [string map [list "{{ utc_dt }}" $utc_dt] $content]
        }
    } else {
	set content_type "text/html"
        set content "File not found: $filepath"
    }

    return [list $content_type $content]
}
