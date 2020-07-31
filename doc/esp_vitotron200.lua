-- LUA script ESP8266
-- VITOTRON 200
-- https://github.com/openv/openv/issues/349


staconfig={}
staconfig.ssid=""
staconfig.pwd=""

wifi.setmode(wifi.STATION)
wifi.sta.config(staconfig)
wifi.sta.connect()

countdown = 6 -- start to put values into sql after 60 secs

--tmr.alarm(1, 10000, 1, function() -- every 10 secs
	--if countdown == 0 then
		--countdown = 30 -- 30 * 10 secs = 5 mins
		--if (wifi.sta.getip() ~= nil) and (t1 ~= "--") then
			--getstr ="http://192.168.1.7/heating.php?t1=" .. t1 .. "&t2=" .. t2 .. "&t3=" .. t3 .. "&t4=" .. t4
			--getstr = getstr .. "&t5=" .. t5 .. "&t6=" .. t6 .. "&t7=" .. t7
			--getstr = getstr .. "&s1=" .. s1 .. "&h1=" .. h1 .. "&sp=" .. sp .. "&pa=" .. pa .. "&br=" .. br
			
			--http.get(getstr, nil, function(code, data)
				--if (code < 0) then 
					--print("HTTP request failed")
				--else 
					--countdown = tonumber(string.match(data,'%d+')) 
				--end
				--if (countdown < 1) or (countdown > 30) then 
					--countdown = 30 
				--end
			--end)	-- http.get
		--end	-- if wifi.sta.getip()
	--end	-- if countdown == 0
	--countdown = countdown - 1
--end)

srv=net.createServer(net.TCP,30)
srv:listen(80,function(conn)
	conn:on("receive", function(client,request)
		buf='HTTP/1.1 200 OK\r\nContent-type: text/html\r\nConnection: close\r\n\r\n'
		buf=buf .. "ESP ID: " .. node.chipid() .." "
		buf=buf .. "heap : " .. node.heap() .." "
		buf=buf .. "cnt : " .. countdown .." "
		buf=buf .. "Aussen1 : " .. t1 .. " "
		buf=buf .. "Kessel1 : " .. t2 .. " "
		buf=buf .. "Speicher1 : " .. t3 .. " "
		buf=buf .. "Ruecklauf1: " .. t4 .. " "
		buf=buf .. "Vorlauf1 : " .. t5 .. "	"
		buf=buf .. "Kessel2 : " .. t6 .. " "
		buf=buf .. "Speicher2 : " .. t7 .. " "
		buf=buf .. "Starts : " .. s1 .. " "
		buf=buf .. "Hours : " .. h1 .. " "
		buf=buf .. "Spar : " .. sp .. "	"
		buf=buf .. "Party : " .. pa .. " "
		buf=buf .. "Brenner : " .. br .. " "
		buf=buf .. ""
		conn:send(buf)
		conn:on("sent",function(conn) conn:close() end)
	end)	--conn:on
end)	--srv:listen



t1 = 0
t2 = 0
t3 = 0
t4 = 0
t5 = 0
t6 = 0
t7 = 0
s1 = 0
h1 = 0
sp = 0
pa = 0
br = 0
uart.setup(0, 4800, 8, uart.PARITY_EVEN, uart.STOPBITS_2, 0)
stat = 0
cnt = 1

uart.on("data", 4, function(data)
	if stat == 0 then 
		if data == string.char(0x05, 0x05, 0x05, 0x05) then -- wait for vitotronic bytes
			stat = 1
			uart.write(0,0x01,0xF7) -- read command
			if cnt == 1 then uart.write(0,0x08, 0x00) end
			if cnt == 2 then uart.write(0,0x08, 0x04) end
			if cnt == 3 then uart.write(0,0x08, 0x0A) end
			if cnt == 4 then uart.write(0,0x08, 0x10) end
			if cnt == 5 then uart.write(0,0x08, 0x8A) end
			if cnt == 6 then uart.write(0,0x08, 0xA7) end
			if cnt == 7 then uart.write(0,0x23, 0x02) end
			if cnt == 8 then uart.write(0,0x55, 0x1E) end
			uart.write(0, 0x04) -- read 4 bytes
		end
	elseif stat == 1 then
		if cnt == 1 then 
			t1 = data:byte(1) + data:byte(2) * 256
			t2 = data:byte(3) + data:byte(4) * 256 
		end
		if cnt == 2 then 
			t3 = data:byte(1) + data:byte(2) * 256 
		end
		if cnt == 3 then 
			t4 = data:byte(1) + data:byte(2) * 256
			t5 = data:byte(3) + data:byte(4) * 256 
		end
		if cnt == 4 then 
			t6 = data:byte(1) + data:byte(2) * 256
			t7 = data:byte(3) + data:byte(4) * 256 
		end
		if cnt == 5 then 
			s1 = data:byte(1) + data:byte(2) * 256 + data:byte(3) * 65536 + data:byte(4) * 16777216 
		end
		if cnt == 6 then 
			h1 = data:byte(1) + data:byte(2) * 256 + data:byte(3) * 65536 + data:byte(4) * 16777216 
		end
		if cnt == 7 then 
			sp = data:byte(1)
			pa = data:byte(2) 
		end
		if cnt == 8 then 
			br = data:byte(1) 
		end
		cnt = cnt + 1
		if cnt > 8 then 
			cnt = 1 
		end
		stat = 0
	end	-- if stat
end, 0)	-- uart.on
