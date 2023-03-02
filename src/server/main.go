package main

import (
	"flag"
	"fmt"
	"io"
	"time"

	"github.com/gin-gonic/gin"
)

// Init data structures to hold queue and subscription info
var queues = make(map[string]chan string)
var subscriptions = make(map[string]map[string]struct{})

// Queue Handler
func queueHandler(c *gin.Context) {
	queueName := c.Param("id")
	// Check if queue is in the system
	if _, exists := queues[queueName]; !exists {
		c.String(404, fmt.Sprintf("There is no queue named %s", queueName))
		return
	}
	// Wait until a message is ready
	func() {
		for {
			select {
			// If a message is ready, send and break
			case message := <-queues[queueName]:
				c.String(200, message)
				return
			default:
				// If there is no message, wait until one is ready
				time.Sleep(1 * time.Second)
			}
		}
	}()
}

// Topic Handler
func topicHandler(c *gin.Context) {
	topic := c.Param("id")
	// Read the request body
	message, err := io.ReadAll(c.Request.Body)
	if err != nil {
		c.String(404, "Bad message")
	}
	// Send message to anyone who is subscribed to the topic
	subscribers := 0
	for queue, topics := range subscriptions {
		if _, exists := topics[topic]; exists {
			// Send the message to the queues channel
			queues[queue] <- string(message)
			subscribers++
		}
	}
	if subscribers == 0 {
		c.String(404, fmt.Sprintf("There are no subscribers for topic %s", topic))
	} else {
		c.String(200, fmt.Sprintf("Published message (%d bytes) to %d subscribers of %s", len(message), subscribers, topic))
	}
}

// Subscription Handler
func subscriptionHandler(c *gin.Context) {
	queueName := c.Param("queue")
	topicName := c.Param("id")
	switch c.Request.Method {
	case "PUT":
		// Check if the queue name is in the system
		if _, exists := queues[queueName]; !exists {
			queues[queueName] = make(chan string, 100)
		}
		// Check if the queue is in the subscriptons map, if not add it
		if _, exists := subscriptions[queueName]; !exists {
			subscriptions[queueName] = make(map[string]struct{})
		}
		// Check if topic already exists in subscriptons map
		if _, exists := subscriptions[queueName][topicName]; exists {
			c.String(404, fmt.Sprintf("Queue %s is already subscribed to topic %s", queueName, topicName))
			return
		}
		subscriptions[queueName][topicName] = struct{}{}
		c.String(200, fmt.Sprintf("Subscribed queue %s to topic %s", queueName, topicName))
		// Queue does not exist in the system, if not add it
	case "DELETE":
		// If queue does not exist, raise an error
		if _, exists := subscriptions[queueName]; !exists {
			c.String(404, fmt.Sprintf("There is no queue named %s", queueName))
			return
		}
		// If queue is not subscribed to the topic, raise an error
		if _, exists := subscriptions[queueName][topicName]; !exists {
			c.String(404, fmt.Sprintf("Queue %s is not subscribed to topic %s", queueName, topicName))
			return
		}
		// Unsubscribe the queue from the topic
		delete(subscriptions[queueName], topicName)
		c.String(200, fmt.Sprintf("Unsubscribed queue %s from topic %s", queueName, topicName))
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
	// Request handlers
	r.PUT("/topic/:id", topicHandler)
	r.Any("/subscription/:queue/:id", subscriptionHandler)
	r.GET("/queue/:id", queueHandler)
	r.Run(fmt.Sprintf("%s:%s", *host, *port))
}
