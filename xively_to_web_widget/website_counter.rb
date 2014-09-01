#!/usr/bin/ruby

require 'rubygems'
require 'net/https'
require 'json'
require 'time'
require 'time_start_and_end_extensions'
require 'xively_keys'
require 'local_config'

# These must match the datastreams in the Xively feed
DatastreamBattery = 0
DatastreamCharge = 1
DatastreamFreeRAM = 2
DatastreamMovement = 3

start_of_month = Time.now.start_of_month
start_of_week = Time.now.start_of_work_week
start_of_day = Time.now.start_of_day
period_start = start_of_month
period_duration = 4

puts "day "+start_of_day.to_s
puts "week "+start_of_week.to_s
puts "month "+start_of_month.to_s

day_visitors = 0
week_visitors = 0
month_visitors = 0

while period_start < Time.now
  puts "Generating data for #{period_start}"

  feed_url = "#{FEED_URL}?start=#{period_start.iso8601}&duration=#{period_duration}hours&interval=0"

  # Get the data from Xively
  data_uri = URI.parse(feed_url)
  data_http = Net::HTTP.new(data_uri.host, 443)
  data_http.use_ssl = true
  data_req = Net::HTTP::Get.new(data_uri.request_uri)
  data_req['X-ApiKey'] = XIVELY_API_KEY
  data_data = data_http.request(data_req)
  
  # Turn it into something we can use
  feed = JSON.parse(data_data.body)
  
  #puts feed.inspect

  # Loop through all the datapoints we've got...
  feed["datastreams"][DatastreamMovement]["datapoints"].each do |v|
    datapoint_time = Time.parse(v["at"])
    
    # ...adding them to the relevant counts
    if datapoint_time > start_of_month
      month_visitors = month_visitors + v["value"].to_i
    end
    if datapoint_time > start_of_week
      week_visitors = week_visitors + v["value"].to_i
    end
    if datapoint_time > start_of_day
      day_visitors = day_visitors + v["value"].to_i
    end
  end

  # Move onto the next time period
  puts
  sleep 1
  period_start = period_start + period_duration*60*60
end

puts "Visitor counts:"
puts "Day: #{day_visitors}"
puts "Week: #{week_visitors}"
puts "Month: #{month_visitors}"

# Output it to the HTML file
File.open(OUTPUT_HTML_FILE, 'w') do |f|
  f.puts <<END_OF_HTML
<html>
  <head>
<!-- Get the main style CSS -->
<link rel="stylesheet" type="text/css" media="all" href="http://www.zone2014.org/wp-content/themes/invictus_3.2.2/style.css" />
<!-- Get the color scheme style CSS -->
<link rel="stylesheet" type="text/css" media="all" href="http://www.zone2014.org/wp-content/themes/invictus_3.2.2/css/black.css" />

<!-- Font include -->
<style type="text/css">

	body {
		font: 14px/20px "Metrophobic"; font-weight: normal;
		color: #ffffff	}

	#showtitle, #slidecaption, #responsiveTitle  {
		font-family: "Metrophobic", "Helvetica Neue", Helvetica, Arial, sans-serif;
		font-weight: 300;
	}

	nav#navigation ul li a {
		color: #ffffff; font: 15px/18px "Metrophobic"; font-weight: 100	}

	nav#navigation ul ul.sub-menu li a  {
		color: #FFFFFF; font: 12px/18px "Metrophobic"; font-weight: 100	}

	h1, h1 a:link, h1 a:visited { color: #db6ddb; font: 42px/60px "Metrophobic"; font-weight: 100; }
	h2 { color: #c73a3a; font: 36px/50px "Metrophobic"; font-weight: 100; }
	h3 { color: #ffffff; font: 30px/40px "Metrophobic"; font-weight: 100; }
	h4 { color: #ffffff; font: 24px/30px "Metrophobic"; font-weight: 100; }
	h5 { color: #ffffff; font: 18px/20px "Metrophobic"; font-weight: 100; }
	h6 { color: #CCCCCC; font: 16px/15px Arial, Helvetica, sans-serif; font-weight:  100; }


	#sidebar h1.widget-title, #sidebar h2.widget-title {
		color: #c73a3a; font: 24px/28px Arial, Helvetica, sans-serif !important; font-weight: 100;
	}

	#welcomeTeaser, #sidebar .max_widget_teaser {
  	font-family: Yanone Kaffeesatz;
  	font-size: 12px;
  	color: #ddd;
  	line-height: 28px;
  	font-weight: 100;
	}

    .blog h2.entry-title,
  .tag h2.entry-title {
		font: 28px/30px Arial, Helvetica, sans-serif; font-weight: 100;
		color: #FFFFFF  }
  
		#welcomeTeaser .inner strong, #sidebar .max_widget_teaser strong { font-size: 110%; }
	
		#showtitle .imagetitle { color: #FFFFFF!important; font: 38px/38px Arial, Helvetica, sans-serif!important; font-weight: 100 !important; }
	
		#showtitle .imagecaption { color: #FFFFFF; font: 18px/18px Arial, Helvetica, sans-serif!important; font-weight: 100 !important; }
	
</style>
<link rel='stylesheet' id='responsive-css'  href='http://www.zone2014.org/wp-content/themes/invictus_3.2.2/css/responsive.css?ver=3.8.4' type='text/css' media='all' />
<link rel='stylesheet' id='custom-css'  href='http://www.zone2014.org/wp-content/themes/invictus_3.2.2/css/custom.css?ver=3.8.4' type='text/css' media='all' />
<link rel='stylesheet' id='headers-css'  href='http://www.zone2014.org/wp-content/themes/invictus_3.2.2/css/headers.css?ver=3.8.4' type='text/css' media='all' />
<link rel='stylesheet' id='fontawesome-css'  href='http://www.zone2014.org/wp-content/themes/invictus_3.2.2/css/font/font-awesome.min.css?ver=3.0.2' type='text/css' media='all' />
<link rel='stylesheet' id='googleFonts_Metrophobic-css'  href='http://fonts.googleapis.com/css?family=Metrophobic%3Aregular&#038;subset=latin%2Clatin-ext&#038;ver=3.8.4' type='text/css' media='all' />

    <style>
      td { text-align: center; padding: 0 0.2em }
    </style>
  </head>
  <body>
    <div align="center">
      <table>
        <tr>
          <td rowspan="2">visitors:</td><td align="center">today</td><td>/</td><td>this week</td><td>/</td><td>this month</td>
        </tr>
        <tr>
          <td>#{day_visitors}</td><td>/</td><td>#{week_visitors}</td><td>/</td><td>#{month_visitors}</td>
        </tr>
      </table>
    </div>
  </body>
</html>
END_OF_HTML
end

