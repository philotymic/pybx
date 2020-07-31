import React, { useState, useRef } from 'react';
import {
  GroupingState,
  IntegratedGrouping,
} from '@devexpress/dx-react-grid';
import { Grid,
	 Table,
	 TableHeaderRow,
	 GroupingPanel,
	 DragDropProvider,
	 Toolbar,
	 TableColumnReordering,
         TableGroupRow
       } from '@devexpress/dx-react-grid-bootstrap4';
import '@devexpress/dx-react-grid-bootstrap4/dist/dx-react-grid-bootstrap4.css';

import * as libdipole from 'libdipole-js';
import * as Blotter from './Blotter.js';

function App() {
    let comm = new libdipole.Communicator();
    const blotter_ptr = useRef(null);
    const [columns, setColumns] = useState([]);
    const [rows, setRows] = useState([]);
    const [columnOrder, setColumnOrder] = useState([]);

    React.useEffect(() => {
	let ws_url = "ws://localhost:8080/";
	let object_id = "test_df";	
	comm.connect(ws_url, object_id).then(o_ptr => {
	    blotter_ptr.current = new Blotter.DFTestPtr(o_ptr);
	});
    }, []);

    const onClick = () => {
	blotter_ptr.current.get_df().then(df_json => {
	    console.log("onClick:", df_json);
	    setColumns(df_json.retval.columns.map(x => { return {name: x}; }));
	    setColumnOrder(df_json.retval.columns);
	    setRows(JSON.parse(df_json.retval.dataframeJSON));
	});
    }
    
    return (<div>
	    <div className="card">
	    <Grid rows={rows} columns={columns}>
	    <GroupingState />
	    <DragDropProvider />
	    <IntegratedGrouping />
	    <Table />
	    <TableHeaderRow />
	    <TableGroupRow />
	    <TableColumnReordering order={columnOrder} onOrderChange={setColumnOrder} />
	    <Toolbar />
            <GroupingPanel />
	    </Grid>
	    </div>
	    <button onClick={onClick}>PRESS</button>
	    </div>);
}

export default App;
