cliApp = function(website) {
    this.url = website;
}

cliApp.prototype = {};

cliApp.prototype.marshall = function(value, type) {
    return marshall_thrift_json(value, type);
}

cliApp.prototype.unmarshall = function(buf, value, type) {
    return unmarshall_thrift_json(buf, value, type);
}

cliApp.prototype.internal_call = function(args) {
    var self = this;
    var ret = null;
    dsn_call(
        this.url,
        "",
        "RPC_CLI_CLI_CALL",
        "POST",
        args+"\0",
        false,
        function(result) {
            ret = result;
        },
        function(xhr, textStatus, errorThrown) {
            ret = null;
        }
    );
    return ret;
}

cliApp.prototype.internal_async_call = function(args, on_success, on_fail) {
    var self = this;
    var ret = null;
    dsn_call(
        this.url,
        "",
        "RPC_CLI_CLI_CALL",
        "POST",
        //this.marshall(args, "struct"),
        args+"\0",
        true,
        function(result) {
            //ret = self.unmarshall(result, null, "string");
            ret = result;
            on_success(ret);
        },
        function(xhr, textStatus, errorThrown) {
            ret = null;
            if (on_fail) {
                on_fail(xhr, textStatus, errorThrown);
            }
        }
    );
    return ret;
}

cliApp.prototype.call = function(obj) {
    if (!obj.async) {
        return this.internal_call(obj.args);
    } else {
        this.internal_async_call(obj.args, obj.on_success, obj.on_fail);
    }
}

