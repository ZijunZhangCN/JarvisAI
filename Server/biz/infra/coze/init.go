package coze

import (
	"github.com/coze-dev/coze-go"
)

var (
	auth   coze.Auth
	Client coze.CozeAPI
)

const (
	Token  = ""                    // TODO: coze token
	BotID  = "7481685145037537317" // TODO: coze 智能体 BotID
	UserID = ""
)

func Init() {
	auth = coze.NewTokenAuth(Token)
	Client = coze.NewCozeAPI(auth, coze.WithBaseURL(coze.CnBaseURL))
}
