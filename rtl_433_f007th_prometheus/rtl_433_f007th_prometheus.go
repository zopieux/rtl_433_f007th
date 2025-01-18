package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/collectors"
	"github.com/prometheus/client_golang/prometheus/promhttp"

	"go.bug.st/serial"
)

type channelMatcherMap map[uint8]string

func (lms channelMatcherMap) Set(m string) error {
	p := strings.SplitN(m, ",", 2)
	if len(p) != 2 {
		panic("")
	}
	channel, err := strconv.ParseInt(p[0], 10, 8)
	if err != nil {
		panic(err)
	}
	lms[uint8(channel)] = p[1]
	return nil
}

func (lms channelMatcherMap) String() string {
	return ""
}

var (
	serialPort      = flag.String("serial", "/dev/ttyACM0", "Serial port to read from")
	addr            = flag.String("listen", ":9550", "Address to listen on")
	channelMatchers = make(channelMatcherMap)
)

var (
	labels          = []string{"id", "channel", "location"}
	packetsReceived = prometheus.NewCounterVec(
		prometheus.CounterOpts{
			Name: "rtl_433_packets_received",
			Help: "Packets (temperature messages) received.",
		},
		labels,
	)
	temperature = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "rtl_433_temperature_celsius",
			Help: "Temperature in Celsius",
		},
		labels,
	)
	humidity = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "rtl_433_humidity",
			Help: "Relative Humidity (0-1.0)",
		},
		labels,
	)
	timestamp = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "rtl_433_timestamp_seconds",
			Help: "Timestamp we received the message (Unix seconds)",
		},
		labels,
	)
	battery = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "rtl_433_battery",
			Help: "Battery high (1) or low (0).",
		},
		labels,
	)
)

type f007thMessage struct {
	Device      uint8   `json:"device"`
	Channel     uint8   `json:"channel"`
	LowBattery  bool    `json:"low_battery"`
	Temperature float64 `json:"temperature_c"`
	Humidity    int     `json:"humidity"`
}

func main() {
	flag.Var(&channelMatchers, "channel_matcher", "1,Bedroom")
	flag.Parse()

	prometheus.MustRegister(packetsReceived, temperature, humidity, timestamp, battery)
	prometheus.MustRegister(collectors.NewBuildInfoCollector())

	go func() {
		http.Handle("/metrics", promhttp.Handler())
		if err := http.ListenAndServe(*addr, nil); err != nil {
			log.Fatal(err)
		}
	}()

	port, err := serial.Open(*serialPort, &serial.Mode{BaudRate: 115200})
	if err != nil {
		log.Fatalf("error opening serial port: %s", err)
	}

	scanner := bufio.NewScanner(port)
	for scanner.Scan() {
		data := scanner.Bytes()
		m := f007thMessage{}
		if err := json.Unmarshal(data, &m); err != nil {
			log.Printf("error unmarshaling: %s", string(data))
			continue
		}
		// log.Printf("received: %+v", m)
		location := channelMatchers[m.Channel]
		labels := []string{strconv.FormatInt(int64(m.Device), 10), strconv.FormatInt(int64(m.Channel), 10), location}
		packetsReceived.WithLabelValues(labels...).Inc()
		timestamp.WithLabelValues(labels...).SetToCurrentTime()
		temperature.WithLabelValues(labels...).Set(m.Temperature)
		humidity.WithLabelValues(labels...).Set(float64(m.Humidity) / 100)
		battery.WithLabelValues(labels...).Set(map[bool]float64{false: 1, true: 0}[m.LowBattery])
	}

	// Exit unsuccessfully since the serial quit on us.
	os.Exit(2)
}
