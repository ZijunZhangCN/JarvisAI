package handler

import (
	"context"
	"errors"
	coze2 "esp32/biz/infra/coze"
	"fmt"
	"github.com/bytedance/gopkg/util/logger"
	"github.com/cloudwego/hertz/pkg/app"
	"github.com/cloudwego/hertz/pkg/common/utils"
	"github.com/cloudwego/hertz/pkg/protocol/consts"
	"github.com/coze-dev/coze-go"
	"github.com/hertz-contrib/websocket"
	"io"
	"strings"
)

var upgrader = websocket.HertzUpgrader{} // use default options

func WSChat(ctx context.Context, c *app.RequestContext) {
	err := upgrader.Upgrade(c, func(conn *websocket.Conn) {
		defer conn.Close()
		userInput := c.Query("input")
		req := &coze.CreateChatsReq{
			BotID:  coze2.BotID,
			UserID: coze2.UserID,
			Messages: []*coze.Message{
				coze.BuildUserQuestionText(userInput, nil),
			},
		}

		resp, err := coze2.Client.Chat.Stream(ctx, req)
		if err != nil {
			logger.CtxErrorf(ctx, "chat stream failed, err: %+v", err)
			return
		}

		defer resp.Close()

		var buffer strings.Builder // 用于缓存接收到的内容
		splitSymbol := []string{"?", "~", "!", "。", "?", "！", "\n"}

		for {
			event, err := resp.Recv()
			if errors.Is(err, io.EOF) {
				logger.CtxInfof(ctx, "stream finished")
				break
			}
			if err != nil {
				fmt.Println(err)
				break
			}

			if event.Event == coze.ChatEventConversationMessageDelta {
				content := event.Message.Content
				buffer.WriteString(content) // 将内容写入缓存

				// 检查是否到达一句话的结尾
				if hasSuffixInSlice(content, splitSymbol) {
					// 发送完整的一句话
					if err = conn.WriteMessage(websocket.TextMessage, []byte(buffer.String())); err != nil {
						logger.CtxErrorf(ctx, "write msg failed, err: %+v", err)
						break
					}
					logger.CtxInfof(ctx, "%s", buffer.String())
					buffer.Reset() // 清空缓存
				}
			} else if event.Event == coze.ChatEventConversationChatCompleted {
				// 如果还有未发送的内容，发送剩余的内容
				if buffer.Len() > 0 {
					if err = conn.WriteMessage(websocket.TextMessage, []byte(buffer.String())); err != nil {
						logger.CtxErrorf(ctx, "write msg failed, err: %+v", err)
						break
					}
					logger.CtxInfof(ctx, "%s", buffer.String())
					buffer.Reset()
				}

				// 发送关闭消息
				if err = conn.WriteMessage(websocket.CloseMessage, websocket.FormatCloseMessage(websocket.CloseNormalClosure, "Server is closing the connection")); err != nil {
					logger.CtxErrorf(ctx, "write msg failed, err: %+v", err)
					break
				}
			} else {
				fmt.Printf("\n")
			}
		}
	})

	if err != nil {
		c.JSON(consts.StatusBadRequest, utils.H{"error": err.Error()})
	}
}

func hasSuffixInSlice(s string, splitSymbolSlice []string) bool {
	for _, suffix := range splitSymbolSlice {
		if strings.HasSuffix(s, suffix) {
			return true
		}
	}
	return false
}
