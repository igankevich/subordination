$( document ).ready(function() {
//    $("div.headertitle").addClass("page-header");
    $("div.title").addClass('h1');

    $('img[src="ftv2ns.png"]').replaceWith('<span class="label label-danger">N</span> ');
    $('img[src="ftv2cl.png"]').replaceWith('<span class="label label-danger">C</span> ');

    $("#navrow1")
		.addClass("collapse navbar-collapse")
		.removeClass('tabs')
		.css('border-bottom', '0px')
		.appendTo('nav .container')
    $("#navrow2").css("clear", "both");
    $("#navrow3").css("clear", "both");
    $("#navrow4").css("clear", "both");
    $("#navrow5").css("clear", "both");
    $("#navrow1 .tablist").addClass("nav navbar-nav");
    $("#navrow2 .tablist").addClass("nav nav-tabs");
    $("#navrow3 .tablist").addClass("nav nav-tabs");
    $(".tablist").removeClass('tablist');
    $("li.current").addClass("active");
    $("iframe").attr("scrolling", "yes");

    $("#nav-path > ul").addClass("breadcrumb");

    $("table.params").addClass("table");
    $("div.ingroups").wrapInner("<small></small>");
    $("div.levels").css("margin", "0.5em");
    $("div.levels > span").addClass("btn btn-default btn-xs");
    $("div.levels > span").css("margin-right", "0.25em");

    $("table.directory").addClass("table table-striped");
    $("div.summary > a").addClass("btn btn-default btn-xs");
    $("table.fieldtable").addClass("table");
    $(".fragment").addClass("well");
    $(".memitem").addClass("panel panel-default");
    $(".memproto").addClass("panel-heading");
    $(".memdoc").addClass("panel-body");
    $("span.mlabel").addClass("label label-info");

    $("table.memberdecls").addClass("table");
    $("[class^=memitem]").addClass("active");

    $("div.ah").addClass("btn btn-default");
    $("span.mlabels").addClass("pull-right");
    $("table.mlabels").css("width", "100%")
    $("td.mlabels-right").addClass("pull-right");

    $("div.ttc").addClass("panel panel-primary");
    $("div.ttname").addClass("panel-heading");
    $("div.ttname a").css("color", 'white');
    $("div.ttdef,div.ttdoc,div.ttdeci").addClass("panel-body");

    $('#MSearchBox').parent().remove();

    $('div.fragment.well div.line:first').css('margin-top', '15px');
    $('div.fragment.well div.line:last').css('margin-bottom', '15px');
	
	$('table.doxtable').removeClass('doxtable').addClass('table table-striped table-bordered').each(function(){
		$(this).prepend('<thead></thead>');
		$(this).find('tbody > tr:first').prependTo($(this).find('thead'));
		
		$(this).find('td > span.success').parent().addClass('success');
		$(this).find('td > span.warning').parent().addClass('warning');
		$(this).find('td > span.danger').parent().addClass('danger');
	});
	
	

    if($('div.fragment.well div.ttc').length > 0)
    {
        $('div.fragment.well div.line:first').parent().removeClass('fragment well');
    }

    $('table.memberdecls').find('.memItemRight').each(function(){
        $(this).contents().appendTo($(this).siblings('.memItemLeft'));
        $(this).siblings('.memItemLeft').attr('align', 'left');
    });
	
	$(".memitem").removeClass('memitem');
    $(".memproto").removeClass('memproto');
    $(".memdoc").removeClass('memdoc');
	$("span.mlabel").removeClass('mlabel');
	$("table.memberdecls").removeClass('memberdecls');
    $("[class^=memitem]").removeClass('memitem');
    $("span.mlabels").removeClass('mlabels');
    $("table.mlabels").removeClass('mlabels');
    $("td.mlabels-right").removeClass('mlabels-right');
	$(".navpath").removeClass('navpath');
	$("li.navelem").removeClass('navelem');
	$("a.el").removeClass('el');
	$("div.ah").removeClass('ah');
	$("div.header").removeClass("header");
	
	$('.mdescLeft').each(function(){
		if($(this).html()=="&nbsp;") {
			$(this).siblings('.mdescRight').attr('colspan', 2);
			$(this).remove();
		}
	});
	$('td.memItemLeft').each(function(){
		if($(this).siblings('.memItemRight').html()=="") {
			$(this).attr('colspan', 2);
			$(this).siblings('.memItemRight').remove();
		}
	});
});
