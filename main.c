//gcc main.c `pkg-config gtk+-3.0 --cflags --libs`
// Разделить на GUI и разбор
#include <gtk/gtk.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdbool.h>

/* columns */
enum
{
	NODE_COLUMN = 0,
	PROP_COLUMN,
	VALUE_COLUMN,
	
	NUM_COLUMNS
};

static GtkTreeStore *model;
static GtkTreeView *tree;
static char *pNextChar;
static sigjmp_buf parse_sigjmp_buf;

static void getNode(GtkTreeIter* parent, int level, const char* parentNodeName, const char* propName);

static const char* propNameWithLevel(int level, const char* parentNodeName, const char* propname) {
	if (level <= 0)
		return propname;

	static char buf[200];
	if (0 == *parentNodeName && 0 == *propname)
		snprintf(buf, sizeof(buf), "%i_", level);
	else
		snprintf(buf, sizeof(buf), "%i_%s.%s", level, parentNodeName, propname);
	return buf;
}

static char getNextChar() {
	if (0 == *pNextChar) {
		fprintf(stderr, "getNextChar\n");
		longjmp(parse_sigjmp_buf, 1);
	}

	return *(pNextChar++);
}

static void skipSpace() {
	while (isspace(getNextChar()));
	pNextChar--;
}

static void getList(GtkTreeIter* parent, int level, const char* parentNodeName, const char* propName) {
	skipSpace();
	if (getNextChar() != '(') {
		fprintf(stderr, "Список должен начинаться с круглой скобки\n");
		longjmp(parse_sigjmp_buf, 1);
	}
	skipSpace();

	GtkTreeIter iter;
	gtk_tree_store_append (model, &iter, parent);
	gtk_tree_store_set (model, &iter,
						NODE_COLUMN, "Список",
						PROP_COLUMN, propNameWithLevel(level, parentNodeName, propName),
						-1);
	if (')' == *pNextChar) {
		getNextChar();
		return;
	}

	if ('{' == *pNextChar) {		
		for (;;) {
			
			if (')' == *pNextChar) {
				getNextChar();
				break;
			}
	
			if ('{' == *pNextChar)
				getNode(&iter, level + 1, "", "");

			skipSpace();
		}
	} else {
		const char* propValue = pNextChar;
		while (getNextChar() != ')');
		*(pNextChar - 1) = '\0';
		gtk_tree_store_set (model, &iter, VALUE_COLUMN, propValue, -1);
	}
}

// true - прочитаны только свойства, false - прочитана завершающая фигурная скобка узла
static bool getNodeProps(GtkTreeIter* parent, int level, const char* parentNodeName) {	
	for (bool bracketIsNotReaded = true; bracketIsNotReaded;) {
		skipSpace();
		if (getNextChar() != ':') {
			pNextChar--;
			return bracketIsNotReaded;
		}
	
		const char* propName = pNextChar;
		while (!isspace(getNextChar()));
		*(pNextChar - 1) = '\0';
	
		skipSpace();
		const char* propValue = pNextChar;
		if ('{' == *propValue)
			getNode(parent, level, parentNodeName, propName);
		else if ('(' == *propValue)
			getList(parent, level, parentNodeName, propName);
		else {
			if (':' == *propValue)
				propValue = "";
			else {
				for (;;) {
					char c = getNextChar();
					if ('}' == c) {
						bracketIsNotReaded = false;
						break;
					}

					if (isspace(c))
						break;
				};

				*(pNextChar - 1) = '\0';
			}

			GtkTreeIter iter;
			gtk_tree_store_append (model, &iter, parent);
			gtk_tree_store_set (model, &iter,
								NODE_COLUMN, "",
								PROP_COLUMN, propNameWithLevel(level, parentNodeName, propName),
								VALUE_COLUMN, propValue,
								-1);
		}
	}
}

static void getNode(GtkTreeIter* parent, int level, const char* parentNodeName, const char* propName) {
	skipSpace();
	if (getNextChar() != '{') {
		fprintf(stderr, "Узел должен начинаться с фигурной скобки\n");
		longjmp(parse_sigjmp_buf, 1);
	}
	skipSpace();

	const char* nodeName = pNextChar;
	while (!isspace(getNextChar()));
	*(pNextChar - 1) = '\0';

	GtkTreeIter iter;
	gtk_tree_store_append (model, &iter, parent);
	gtk_tree_store_set (model, &iter,
						NODE_COLUMN, nodeName,
						PROP_COLUMN, propNameWithLevel(level, parentNodeName, propName),
						VALUE_COLUMN, "",
						-1);	
	if (getNodeProps(&iter, level + 1, nodeName)) {
		skipSpace();
		if (getNextChar() != '}')
		{
			fprintf(stderr, "Узел должен заканчиваться фигурной скобкой");
			longjmp(parse_sigjmp_buf, 1);
		}
	}
}

static void bntParseClicked(GtkButton *button, GtkTextView *textview) {
	char* str;
	g_object_get(gtk_text_view_get_buffer(textview), "text", &str, NULL);
	printf("%s\n", str);
	pNextChar = str;
	gtk_tree_store_clear(model);
	if (setjmp(parse_sigjmp_buf) == 0) {
		getNode(NULL, 0, "", "");
	}
	g_free(str);
	gtk_tree_view_expand_all(tree);
}

int main(int argc, char **argv) {
	gtk_init(&argc, &argv);

	GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "Просмотр дерева узлов");
	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtk_box_pack_start (GTK_BOX (vbox), gtk_label_new ("Введите вывод nodeToString"), FALSE, FALSE, 0);

	GtkWidget *vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
		GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
			GtkWidget *sw_text = gtk_scrolled_window_new (NULL, NULL);
				GtkWidget *textview = gtk_text_view_new ();
					gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_CHAR);
					gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview)),
							"{PLANNEDSTMT :commandType 1 :planGen 1 :queryId 0 :hasReturning false :hasModifyingCTE false :canSetTag true :transientPlan false :oneoffPlan false :simplyUpdatable false :planTree {MOTION :motionID 1 :motionType 1 :sendSorted false :hashExprs <> :hashFuncs :isBroadcast 0 :numSortCols 0 :sortColIdx :sortOperators :collations :segidColIdx 0 :plan_node_id 1 :startup_cost 0.00 :total_cost 431.00 :plan_rows 1 :plan_width 4 :targetlist ({TARGETENTRY :expr {VAR :varno 65001 :varattno 1 :vartype 23 :vartypmod -1 :varcollid 0 :varlevelsup 0 :varnoold 1 :varoattno 1 :location -1} :resno 1 :resname i :ressortgroupref 0 :resorigtbl 16384 :resorigcol 1 :resjunk false}) :qual <> :extParam (b) :allParam (b) :flow <> :dispatch 2 :nMotionNodes 1 :nInitPlans 0 :sliceTable <> :lefttree {SEQSCAN :plan_node_id 2 :startup_cost 0.00 :total_cost 431.00 :plan_rows 1 :plan_width 4 :targetlist ({TARGETENTRY :expr {VAR :varno 1 :varattno 1 :vartype 23 :vartypmod -1 :varcollid 0 :varlevelsup 0 :varnoold 1 :varoattno 1 :location -1} :resno 1 :resname i :ressortgroupref 0 :resorigtbl 16384 :resorigcol 1 :resjunk false}) :qual <> :extParam (b) :allParam (b) :flow {FLOW :flotype 0 :req_move 0 :locustype 0 :segindex 0 :numsegments 1 :hashExprs <> :hashOpfamilies <> :flow_before_req_move <>} :dispatch 0 :nMotionNodes 0 :nInitPlans 0 :sliceTable <> :lefttree <> :righttree <> :initPlan <> :operatorMemKB 0 :scanrelid 1} :righttree <> :initPlan <> :operatorMemKB 0} :rtable ({RTE :alias <> :eref {ALIAS :aliasname t :colnames (\"i\")} :rtekind 0 :relid 16384 :relkind 000 :lateral false :inh false :inFromCl true :requiredPerms 2 :checkAsUser 0 :selectedCols (b) :modifiedCols (b) :forceDistRandom false :securityQuals <>}) :resultRelations <> :utilityStmt <> :subplans <> :rewindPlanIDs (b) :result_partitions <> :result_aosegnos <> :queryPartOids <> :queryPartsMetadata <> :numSelectorsPerScanId <> :rowMarks <> :relationOids (o 16384 16384) :invalItems <> :nParamExec 0 :nMotionNodes 1 :nInitPlans 0 :intoPolicy <> :query_mem 131072000 :intoClause <> :copyIntoClause <> :refreshClause <> :metricsQueryType 0 :total_memory_master 0 :nsegments_master 0}",
							-1);
				gtk_container_add (GTK_CONTAINER (sw_text), textview);
			gtk_box_pack_start (GTK_BOX (hbox), sw_text, TRUE, TRUE, 0);
	
			GtkWidget *btnParse = gtk_button_new_with_label ("Разбор");
				g_signal_connect (btnParse, "clicked",  G_CALLBACK (bntParseClicked), textview);
			gtk_box_pack_start (GTK_BOX (hbox), btnParse, FALSE, FALSE, 0);
		gtk_paned_add1 (GTK_PANED (vpaned), hbox);

		GtkWidget *sw_tree = gtk_scrolled_window_new (NULL, NULL);
			gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw_tree), GTK_SHADOW_ETCHED_IN);
			gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw_tree), GTK_POLICY_AUTOMATIC,  GTK_POLICY_AUTOMATIC);

			GtkWidget *treeview = gtk_tree_view_new_with_model(
				GTK_TREE_MODEL(model = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING)));

				gtk_tree_view_insert_column_with_attributes (tree = GTK_TREE_VIEW (treeview),
					-1, "Узел",
					gtk_cell_renderer_text_new (), "text",
					NODE_COLUMN,
					NULL);

				gtk_tree_view_insert_column_with_attributes (tree,
					-1, "Свойство",
					gtk_cell_renderer_text_new (), "text",
					PROP_COLUMN,
					NULL);

				gtk_tree_view_insert_column_with_attributes (tree,
					-1, "Значение",
					gtk_cell_renderer_text_new (), "text",
					VALUE_COLUMN,
					NULL);

			gtk_container_add (GTK_CONTAINER (sw_tree), treeview);
		gtk_paned_add2 (GTK_PANED (vpaned), sw_tree);
	gtk_box_pack_start (GTK_BOX (vbox), vpaned, TRUE, TRUE, 0);

	gtk_window_set_default_size (GTK_WINDOW (window), 650, 400);
	gtk_window_maximize(GTK_WINDOW (window));
	gtk_widget_show_all(window);
	gtk_main();
	return 0;
}

/*

Разбор начинается с нажатия кнопки

{PLANNEDSTMT :commandType 1 :planGen 1 :queryId 0 :hasReturning false :hasModifyingCTE false :canSetTag true :transientPlan false :oneoffPlan false :simplyUpdatable false :planTree {MOTION :motionID 1 :motionType 1 :sendSorted false :hashExprs <> :hashFuncs :isBroadcast 0 :numSortCols 0 :sortColIdx :sortOperators :collations :segidColIdx 0 :plan_node_id 1 :startup_cost 0.00 :total_cost 431.00 :plan_rows 1 :plan_width 4 :targetlist ({TARGETENTRY :expr {VAR :varno 65001 :varattno 1 :vartype 23 :vartypmod -1 :varcollid 0 :varlevelsup 0 :varnoold 1 :varoattno 1 :location -1} :resno 1 :resname i :ressortgroupref 0 :resorigtbl 16384 :resorigcol 1 :resjunk false}) :qual <> :extParam (b) :allParam (b) :flow <> :dispatch 2 :nMotionNodes 1 :nInitPlans 0 :sliceTable <> :lefttree {SEQSCAN :plan_node_id 2 :startup_cost 0.00 :total_cost 431.00 :plan_rows 1 :plan_width 4 :targetlist ({TARGETENTRY :expr {VAR :varno 1 :varattno 1 :vartype 23 :vartypmod -1 :varcollid 0 :varlevelsup 0 :varnoold 1 :varoattno 1 :location -1} :resno 1 :resname i :ressortgroupref 0 :resorigtbl 16384 :resorigcol 1 :resjunk false}) :qual <> :extParam (b) :allParam (b) :flow {FLOW :flotype 0 :req_move 0 :locustype 0 :segindex 0 :numsegments 1 :hashExprs <> :hashOpfamilies <> :flow_before_req_move <>} :dispatch 0 :nMotionNodes 0 :nInitPlans 0 :sliceTable <> :lefttree <> :righttree <> :initPlan <> :operatorMemKB 0 :scanrelid 1} :righttree <> :initPlan <> :operatorMemKB 0} :rtable ({RTE :alias <> :eref {ALIAS :aliasname t :colnames (\"i\")} :rtekind 0 :relid 16384 :relkind 000 :lateral false :inh false :inFromCl true :requiredPerms 2 :checkAsUser 0 :selectedCols (b) :modifiedCols (b) :forceDistRandom false :securityQuals <>}) :resultRelations <> :utilityStmt <> :subplans <> :rewindPlanIDs (b) :result_partitions <> :result_aosegnos <> :queryPartOids <> :queryPartsMetadata <> :numSelectorsPerScanId <> :rowMarks <> :relationOids (o 16384 16384) :invalItems <> :nParamExec 0 :nMotionNodes 1 :nInitPlans 0 :intoPolicy <> :query_mem 131072000 :intoClause <> :copyIntoClause <> :refreshClause <> :metricsQueryType 0 :total_memory_master 0 :nsegments_master 0}

{PLANNEDSTMT
	:commandType 1
	:planGen 1
	:queryId 0
	:hasReturning false
	:hasModifyingCTE false
	:canSetTag true
	:transientPlan false
	:oneoffPlan false
	:simplyUpdatable false
	:planTree
		{MOTION
			:motionID 1
			:motionType 1
			:sendSorted false
			:hashExprs <> 
			:hashFuncs
			:isBroadcast 0
			:numSortCols 0
			:sortColIdx
			:sortOperators
			:collations
			:segidColIdx 0
			:plan_node_id 1
			:startup_cost 0.00
			:total_cost 431.00
			:plan_rows 1
			:plan_width 4
			:targetlist (
				{TARGETENTRY
					:expr
						{VAR 
							:varno 65001
							:varattno 1
							:vartype 23
							:vartypmod -1
							:varcollid 0
							:varlevelsup 0
							:varnoold 1
							:varoattno 1
							:location -1}
					:resno 1
					:resname i
					:ressortgroupref 0
					:resorigtbl 16384
					:resorigcol 1
					:resjunk false}
				)
			:qual <> :extParam (b) :allParam (b) :flow <> :dispatch 2 :nMotionNodes 1 :nInitPlans 0 :sliceTable <> :lefttree {SEQSCAN :plan_node_id 2 :startup_cost 0.00 :total_cost 431.00 :plan_rows 1 :plan_width 4 :targetlist ({TARGETENTRY :expr {VAR :varno 1 :varattno 1 :vartype 23 :vartypmod -1 :varcollid 0 :varlevelsup 0 :varnoold 1 :varoattno 1 :location -1} :resno 1 :resname i :ressortgroupref 0 :resorigtbl 16384 :resorigcol 1 :resjunk false}) :qual <> :extParam (b) :allParam (b) :flow {FLOW :flotype 0 :req_move 0 :locustype 0 :segindex 0 :numsegments 1 :hashExprs <> :hashOpfamilies <> :flow_before_req_move <>} :dispatch 0 :nMotionNodes 0 :nInitPlans 0 :sliceTable <> :lefttree <> :righttree <> :initPlan <> :operatorMemKB 0 :scanrelid 1} :righttree <> :initPlan <> :operatorMemKB 0} :rtable ({RTE :alias <> :eref {ALIAS :aliasname t :colnames (\"i\")} :rtekind 0 :relid 16384 :relkind 000 :lateral false :inh false :inFromCl true :requiredPerms 2 :checkAsUser 0 :selectedCols (b) :modifiedCols (b) :forceDistRandom false :securityQuals <>}) :resultRelations <> :utilityStmt <> :subplans <> :rewindPlanIDs (b) :result_partitions <> :result_aosegnos <> :queryPartOids <> :queryPartsMetadata <> :numSelectorsPerScanId <> :rowMarks <> :relationOids (o 16384 16384) :invalItems <> :nParamExec 0 :nMotionNodes 1 :nInitPlans 0 :intoPolicy <> :query_mem 131072000 :intoClause <> :copyIntoClause <> :refreshClause <> :metricsQueryType 0 :total_memory_master 0 :nsegments_master 0}

*/

