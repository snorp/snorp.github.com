
const _FIVE_MINUTES = 1000 * 60 * 5;

var userPattern = /@([a-zA-Z0-9]+) /g;
var hashPattern = /\#([a-zA-Z0-9]+)/g;
var linkPattern = /(http\:\/\/[a-zA-Z0-9\/\.]+)/g;

function linkifyTweet(text) {
    text = text.replace(linkPattern, "<a href='$1' target='_blank'>$1</a>");
    text = text.replace(userPattern, "<a href='http://twitter.com/$1' target='_blank'>@$1</a> ");
    text = text.replace(hashPattern, "<a href='http://twitter.com/search?q=%23$1' target='_blank'>#$1</a>")
    return text;
}

function showTweets(items) {
    if (!items)
        return;

    $("#tweet_container").html("<ul></ul>");

    $.each(items, function(i,item) {
        var tweet = document.createElement('li');
        $(tweet).html(linkifyTweet(item.text));
        $(tweet).addClass('tweet');
        $(tweet).data('tweet-id', item.id_str);
        $("#tweet_container ul").append(tweet);
    });

    $(".tweet").click(function(event) {
        var tweet_id = $(event.target).data('tweet-id');

        if (tweet_id) {
            window.open('http://twitter.com/snorp/status/' + tweet_id);
        }
    });
}

function refreshTweets() {
    console.log("Refreshing tweets");
    $.getJSON("http://twitter.com/status/user_timeline/snorp.json?count=5&callback=?", function(data){
        var tweets = { last_fetched: new Date(), items: data };
        localStorage.setItem("tweets", JSON.stringify(tweets));
        showTweets(tweets.items);
    });
}

function maybeRefreshTweets() {
    var tweetJson = localStorage.getItem("tweets");
    if (!tweetJson) {
        refreshTweets();
        return;
    }

    var tweets = JSON.parse(tweetJson);
    var now = new Date();
    var last_fetched = new Date(tweets.last_fetched);

    if ((now - last_fetched) > _FIVE_MINUTES) {
        refreshTweets();
    } else {
        console.log("Showing cached tweets");
        showTweets(tweets.items);
    }
}

function connectEvents() {
    $("#searchbox").focus(function(event) {
        $(this).val("");
    });
    $("#searchform").submit(function(event) {
        window.location = $(this).attr('action') + "?q=site:snorp.net+" + $("#searchbox").val();
        return false;
    });
}

// Load jQuery
google.load("jquery", "1");

google.setOnLoadCallback(function() {
    connectEvents();
    maybeRefreshTweets();
})
