package main

import (
	"flag"
	"fmt"
	"io"
	"time"

	"github.com/gin-gonic/gin"
)

// Structs to bind URI Paths
type slugInfo struct {
	ID string `uri:"id" binding:"required"`
}
type subscriptionInfo struct {
	QUEUE string `uri:"queue" binding:"required"`
	TOPIC string `uri:"id" binding:"required"`
}

// Init data structures to hold queue and subscription info
var queues = make(map[string]chan string)
var subscriptions = make(map[string]map[string]struct{})

// Queue Handler
func queueHandler(c *gin.Context) {
	var queueName slugInfo
	if err := c.ShouldBindUri(&queueName); err != nil {
		c.JSON(400, gin.H{"error": err.Error()})
		return
	}
	// Check if queue is in the system
	if _, ok := queues[queueName.ID]; !ok {
		c.String(404, fmt.Sprintf("There is no queue named %s", queueName.ID))
		return
	}
	// Wait until a message is ready
	loop:
		for {
			select {
			// If a message is ready, send and break
			case message := <- queues[queueName.ID]:
				c.String(200, message)
				break loop
			default:
				// If there is no message, wait until one is ready
				time.Sleep(1 * time.Second)
			}
		}
}

// Topic Handler
func topicHandler(c *gin.Context) {
	var topicName slugInfo
	if err := c.ShouldBindUri(&topicName); err != nil {
		c.JSON(400, gin.H{"error": err})
		return
	}
	message, err := io.ReadAll(c.Request.Body)
	if err != nil {
		c.String(404, "Bad message")
	}

	// Send message to anyone who is subscribed to the topic
	subscribers := 0
	for queue, topics := range subscriptions {
		if _, ok := topics[topicName.ID]; ok {
			// Send the message to the queues channel
			queues[queue] <- string(message)
			subscribers++
		}
	}
	if subscribers == 0 {
		c.String(404, fmt.Sprintf("There are no subscribers for topic %s", topicName.ID))
	} else {
		c.String(200, fmt.Sprintf("Published message (%d bytes) to %d subscribers of %s", len(message), subscribers, topicName.ID))
	}
}

// Subscription Handler
func subscriptionHandler(c *gin.Context) {
	var subRequest subscriptionInfo
	if err := c.ShouldBindUri(&subRequest); err != nil {
		c.JSON(400, gin.H{"error": err})
		return
	}
	switch c.Request.Method {
	case "PUT":
		// Queue does not exist in the system, if not add it
		if _, ok := queues[subRequest.QUEUE]; !ok {
			// var pointer queueBuffer
			// pointer.queue = make(chan string, 100)
			// pointer.messages = make([]string, 0)
			queues[subRequest.QUEUE] = make(chan string, 100)
		}
		// Check if the queue is in the subscriptons map, if not add it
		if _, ok := subscriptions[subRequest.QUEUE]; !ok {
			subscriptions[subRequest.QUEUE] = make(map[string]struct{})
		}
		// Check if topic already exists in subscriptons map
		if _, ok := subscriptions[subRequest.QUEUE][subRequest.TOPIC]; ok {
			c.String(404, fmt.Sprintf("Queue %s is already subscribed to topic %s", subRequest.QUEUE, subRequest.TOPIC))
			return
		}
		subscriptions[subRequest.QUEUE][subRequest.TOPIC] = struct{}{}
		c.String(200, fmt.Sprintf("Subscribed queue %s to topic %s", subRequest.QUEUE, subRequest.TOPIC))
	case "DELETE":
		// If queue does not exist, raise an error
		if _, ok := subscriptions[subRequest.QUEUE]; !ok {
			c.String(404, fmt.Sprintf("There is no queue named %s", subRequest.QUEUE))
			return
		}
		// If queue is not subscribed to the topic, raise an error
		if _, ok := subscriptions[subRequest.QUEUE][subRequest.TOPIC]; !ok {
			c.String(404, fmt.Sprintf("Queue %s is not subscribed to topic %s", subRequest.QUEUE, subRequest.TOPIC))
			return
		}
		// Unsubscribe the queue from the topic
		delete(subscriptions[subRequest.QUEUE], subRequest.TOPIC)
		c.String(200, fmt.Sprintf("Unsubscribed queue %s from topic %s", subRequest.QUEUE, subRequest.TOPIC))
	default:
		c.String(405, "Method not allowed")
	}
}

func main() {
	// Init host and port
	host := flag.String("h", "localhost", "host of server")
	port := flag.String("p", "8080", "port of server")
	flag.Parse()

	// Init gin server
	r := gin.Default()

	// Topic Handler
	r.PUT("/topic/:id", topicHandler)
	// Subscription Handler
	r.Any("/subscription/:queue/:id", subscriptionHandler)
	// Queue Handler
	r.GET("/queue/:id", queueHandler)

	r.Run(fmt.Sprintf("%s:%s", *host, *port))
}
