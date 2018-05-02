var vm = new Vue({
    el: '#app',
    data:{
        setting_list: [
            {'name':'target_server','label':'Server','description':'address of target machine which we care about its runtime information','value':window.location.hostname + ':24601'},
            {'name':'target_meta_server','label':'Meta Server','description':'address of meta server','value':window.location.hostname + ':24602'},
            {'name':'target_app_store','label':'App Store Server','description':'address of app store','value':''},
        ],
        state: ''
    }, 
    watch: {
        'setting_list': {
            handler: function (lst, oldLst) {
                var self = this;
                var index;
                self.state = 'saving';
                for(index = 0; index < lst.length; ++index)
                {
                    localStorage.setItem(lst[index].name, lst[index].value);
                }
                setTimeout(function(){ self.state = ''; }, 1000);
            },
            deep: true
        },
    },
    components: {
    },
    methods: {
        
    },
    ready: function ()
    {        
        var index;
        for(index = 0; index < this.setting_list.length; ++index)
        {
            if (localStorage.getItem(this.setting_list[index].name) == null
                || localStorage.getItem(this.setting_list[index].name) == "")
                localStorage.setItem(this.setting_list[index].name, this.setting_list[index].value);

            this.setting_list[index].value = localStorage[this.setting_list[index].name];
        }
    }
});

